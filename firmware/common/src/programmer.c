/*
 * programmer.cpp
 *
 *  Created on: 29 mar 2023
 *      Author: bielski.j@gmail.com
 */
#include <stdlib.h>
#include <string.h>

#include "protocol.h"
#include "crc8.h"

#include "programmer.h"


typedef enum _RxState {
	RX_STATE_WAIT_SYNC,
	RX_STATE_WAIT_CMD,

	RX_STATE_WAIT_DATA_LEN_HI,
	RX_STATE_WAIT_DATA_LEN_LO,

	RX_STATE_WAIT_DATA,

	RX_STATE_WAIT_CRC
} RxState;


typedef struct _Context {
	ProgrammerSetupParameters params;

	uint16_t idleCounter;
	RxState  rxState;

	uint8_t  pendingCmdCrc;
	uint8_t  pendingCmd;

	uint16_t pendingDataSize;
	uint16_t pendingDataRead;

	uint16_t toSend;
	uint16_t toRecv;
} Context;


static Context *_ctx = NULL;


static void _resetRx() {
	_ctx->pendingCmd    = 0xff;
	_ctx->pendingCmdCrc = 0;

	_ctx->toRecv = 0;
	_ctx->toSend = 0;

	_ctx->pendingDataRead = 0;
	_ctx->pendingDataSize = 0;

	_ctx->rxState = RX_STATE_WAIT_SYNC;
}


static void _response(uint8_t code, uint8_t *buffer, uint16_t bufferSize) {
	SerialSendCallback sendCb     = _ctx->params.serialSendCallback;
	void              *sendCbData = _ctx->params.callbackData;

	if (sendCb) {
		uint8_t crc = PROTO_CRC8_START;

		sendCb(PROTO_SYNC_BYTE, sendCbData);
		crc = crc8_getForByte(PROTO_SYNC_BYTE, PROTO_CRC8_POLY, crc);

		sendCb(code, sendCbData);
		crc = crc8_getForByte(code, PROTO_CRC8_POLY, crc);

		sendCb(bufferSize >> 8, sendCbData);
		crc = crc8_getForByte(bufferSize >> 8, PROTO_CRC8_POLY, crc);

		sendCb(bufferSize & 0xff, sendCbData);
		crc = crc8_getForByte(bufferSize & 0xff, PROTO_CRC8_POLY, crc);

		for (uint16_t i = 0; i < bufferSize; i++) {
			sendCb(buffer[i], sendCbData);
			crc = crc8_getForByte(buffer[i], PROTO_CRC8_POLY, crc);
		}

		sendCb(crc, sendCbData);

		if (_ctx->params.serialFlushCallback) {
			_ctx->params.serialFlushCallback(sendCbData);
		}
	}

	_resetRx();
}


bool programmer_setup(ProgrammerSetupParameters *parameters) {
	if (
		(parameters->memory == NULL) &&
		(parameters->memorySize < (sizeof(Context) + 5)) // TODO: FIXME!
	) {
		return false;
	}

	{
		_ctx = (Context *) parameters->memory;

		memcpy(&_ctx->params, parameters, sizeof(*parameters));

		_ctx->params.memory      = _ctx->params.memory + sizeof(Context);
		_ctx->params.memorySize -= sizeof(Context);

		_ctx->idleCounter = 0;

		_resetRx();
	}

	return true;
}


void programmer_onByte(uint8_t byte) {
	if (_ctx) {
		_ctx->idleCounter = 0;

		switch (_ctx->rxState) {
			case RX_STATE_WAIT_SYNC:
				{
					if (byte != PROTO_SYNC_BYTE) {
						_response(PROTO_ERROR_INVALID_SYNC_BYTE, NULL, 0);

					} else {
						_ctx->rxState       = RX_STATE_WAIT_CMD;
						_ctx->pendingCmdCrc = crc8_getForByte(byte, PROTO_CRC8_POLY, PROTO_CRC8_START);
					}
				}
				break;

			case RX_STATE_WAIT_CMD:
				{
					_ctx->pendingCmd    = byte;
					_ctx->pendingCmdCrc = crc8_getForByte(byte, PROTO_CRC8_POLY, _ctx->pendingCmdCrc);
					_ctx->rxState       = RX_STATE_WAIT_DATA_LEN_HI;
				}
				break;

			case RX_STATE_WAIT_DATA_LEN_HI:
				{
					_ctx->pendingCmdCrc = crc8_getForByte(byte, PROTO_CRC8_POLY, _ctx->pendingCmdCrc);
					_ctx->rxState       = RX_STATE_WAIT_DATA_LEN_LO;

					_ctx->pendingDataSize = ((uint16_t)(byte)) << 8;
				}
				break;

			case RX_STATE_WAIT_DATA_LEN_LO:
				{
					_ctx->pendingDataSize |= byte;

					// FIXME - minimal memory size check!
					if (_ctx->pendingDataSize > _ctx->params.memorySize - 4) {
						_response(PROTO_ERROR_INVALID_LENGTH, NULL, 0);

					} else {
						_ctx->pendingCmdCrc = crc8_getForByte(byte, PROTO_CRC8_POLY, _ctx->pendingCmdCrc);

						if (_ctx->pendingDataSize == 0) {
							_ctx->rxState = RX_STATE_WAIT_CRC;

						} else {
							_ctx->rxState = RX_STATE_WAIT_DATA;

							_ctx->pendingDataRead = 0;
						}
					}
				}
				break;

			case RX_STATE_WAIT_DATA:
				{
					if (_ctx->pendingCmd == PROTO_CMD_SPI_TRANSFER) {
						switch (_ctx->pendingDataRead) {
							case 0:
								{
									_ctx->toSend  = ((uint16_t) byte) << 8;
								}
								break;

							case 1:
								{
									_ctx->toSend |= byte;
								}
								break;

							case 2:
								{
									_ctx->toRecv  = ((uint16_t) byte) << 8;
								}
								break;

							case 3:
								{
									_ctx->toRecv |= byte;
								}
								break;

							default:
								// TODO: Fixme!
								((uint8_t *) _ctx->params.memory)[_ctx->pendingDataRead - 4] = byte;
						}
					}

					if (++_ctx->pendingDataRead == _ctx->pendingDataSize) {
						_ctx->rxState = RX_STATE_WAIT_CRC;
					}

					_ctx->pendingCmdCrc = crc8_getForByte(byte, PROTO_CRC8_POLY, _ctx->pendingCmdCrc);
				}
				break;

			case RX_STATE_WAIT_CRC:
				{
					if (_ctx->pendingCmdCrc != byte) {
						_response(PROTO_ERROR_INVALID_CRC, NULL, 0);

					} else {
						switch (_ctx->pendingCmd) {
							case PROTO_CMD_SPI_CS_HI:
								{
									_ctx->params.spiCsCallback(false, _ctx->params.callbackData);

									_response(PROTO_NO_ERROR, NULL, 0);
								}
								break;

							case PROTO_CMD_SPI_CS_LO:
								{
									_ctx->params.spiCsCallback(true, _ctx->params.callbackData);

									_response(PROTO_NO_ERROR, NULL, 0);
								}
								break;

							case PROTO_CMD_SPI_TRANSFER:
								{
									_ctx->params.spiTransferCallback(_ctx->params.memory, _ctx->toSend, _ctx->toRecv, _ctx->params.callbackData);

									_response(PROTO_NO_ERROR, (uint8_t *) _ctx->params.memory, _ctx->toRecv);
								}
								break;

							default:
								{
									_response(PROTO_ERROR_INVALID_CMD, NULL, 0);
								}
								break;
						}
					}
				}
				break;
		}
	}
}


void programmer_onData(uint8_t *data, uint16_t dataSize) {
	while (dataSize) {
		programmer_onByte(*data);

		dataSize--;
		data++;
	}
}


void programmer_onIdle() {
	if (_ctx) {
		_ctx->idleCounter++;

		if (_ctx->idleCounter >= 60000) {
			if (_ctx->rxState != RX_STATE_WAIT_SYNC) {
				_response(PROTO_ERROR_TIMEOUT, NULL, 0);

				_ctx->rxState = RX_STATE_WAIT_SYNC;
			}

			_ctx->idleCounter = 0;
		}
	}
}
