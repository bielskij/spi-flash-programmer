/*
 * programmer.cpp
 *
 *  Created on: 29 mar 2023
 *      Author: bielski.j@gmail.com
 */
#include <stdlib.h>
#include <string.h>

#include "common/protocol/server.h"
#include "common/protocol.h"
#include "common/crc8.h"


typedef enum _RxState {
	RX_STATE_WAIT_SYNC,
	RX_STATE_ID,
	RX_STATE_VLEN_HI,
	RX_STATE_VLEN_LO,
	RX_STATE_CHECK_PAYLOAD,
	RX_STATE_PAYLOAD,
	RX_STATE_CRC
} RxState;


typedef struct _Context {
	ProtoSrvSetupParameters params;

	uint16_t idleCounter;
	RxState  rxState;

	uint8_t frameCrc;
	uint8_t frameCmd;
	uint8_t frameId;

	uint16_t frameDataSize;
	uint16_t frameDataRead;
} Context;


static Context *_ctx = NULL;


static void _resetRx() {
	_ctx->frameCmd = 0xff;
	_ctx->frameId  = 9;
	_ctx->frameCrc = PROTO_CRC8_START;

	_ctx->frameDataRead = 0;
	_ctx->frameDataSize = 0;

	_ctx->rxState = RX_STATE_WAIT_SYNC;
}


static void _response(uint8_t code, uint8_t *buffer, uint16_t bufferSize) {
	ProtoSrvIoSendCallback sendCb     = _ctx->params.ioSendCallback;
	void                 *sendCbData = _ctx->params.callbackData;

	if (sendCb) {
		uint8_t crc = PROTO_CRC8_START;

		code |= PROTO_SYNC_NIBBLE;

		sendCb(code, sendCbData);
		crc = crc8_getForByte(code, PROTO_CRC8_POLY, crc);

		sendCb(_ctx->frameId, sendCbData);
		crc = crc8_getForByte(_ctx->frameId, PROTO_CRC8_POLY, crc);

		if (bufferSize > 127) {
			code = 0x80 | (bufferSize >> 8);

			sendCb(code, sendCbData);
			crc = crc8_getForByte(code, PROTO_CRC8_POLY, crc);
		}

		sendCb(bufferSize & 0xff, sendCbData);
		crc = crc8_getForByte(bufferSize & 0xff, PROTO_CRC8_POLY, crc);

		for (uint16_t i = 0; i < bufferSize; i++) {
			sendCb(buffer[i], sendCbData);
			crc = crc8_getForByte(buffer[i], PROTO_CRC8_POLY, crc);
		}

		sendCb(crc, sendCbData);

		if (_ctx->params.ioFlushCallback) {
			_ctx->params.ioFlushCallback(sendCbData);
		}
	}

	_resetRx();
}


bool proto_srv_setup(ProtoSrvSetupParameters *parameters) {
	if (
		(parameters->memory == NULL) &&
		(parameters->memorySize < (sizeof(Context) + PROTO_FRAME_MIN_SIZE))
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


void proto_srv_onByte(uint8_t byte) {
	if (_ctx) {
		_ctx->idleCounter = 0;

		switch (_ctx->rxState) {
			case RX_STATE_WAIT_SYNC:
				{
					if ((byte & PROTO_SYNC_NIBBLE_MASK) != PROTO_SYNC_NIBBLE) {
						_response(PROTO_ERROR_INVALID_SYNC_BYTE, NULL, 0);

					} else {
						_ctx->frameCmd = PROTO_CMD_NIBBLE_MASK & byte;
						_ctx->rxState  = RX_STATE_ID;
						_ctx->frameCrc = crc8_getForByte(byte, PROTO_CRC8_POLY, PROTO_CRC8_START);
					}
				}
				break;

			case RX_STATE_ID:
				{
					_ctx->frameId  = byte;
					_ctx->rxState  = RX_STATE_VLEN_HI;
					_ctx->frameCrc = crc8_getForByte(byte, PROTO_CRC8_POLY, _ctx->frameCrc);
				}
				break;

			case RX_STATE_VLEN_HI:
				{
					_ctx->frameDataSize = byte;

					if (_ctx->frameDataSize == 0) {
						_ctx->rxState = RX_STATE_CRC;

					} else {
						if ((_ctx->frameDataSize & 0x80) != 0) {
							_ctx->rxState = RX_STATE_VLEN_LO;

							_ctx->frameDataSize  &= 0x80;
							_ctx->frameDataSize <<= 8;

						} else {
							_ctx->rxState = RX_STATE_CHECK_PAYLOAD;
						}
					}

					_ctx->frameCrc = crc8_getForByte(byte, PROTO_CRC8_POLY, _ctx->frameCrc);
				}
				break;

			case RX_STATE_VLEN_LO:
				{
					_ctx->frameDataSize |= byte;

					if (_ctx->frameDataSize > 0) {
						_ctx->rxState = RX_STATE_CHECK_PAYLOAD;

					} else {
						_ctx->rxState = RX_STATE_CRC;
					}

					_ctx->frameCrc = crc8_getForByte(byte, PROTO_CRC8_POLY, _ctx->frameCrc);
				}
				break;

			case RX_STATE_PAYLOAD:
				{
					((uint8_t *) _ctx->params.memory)[_ctx->frameDataRead++] = byte;

					if (_ctx->frameDataRead == _ctx->frameDataSize) {
						_ctx->rxState = RX_STATE_CRC;
					}

					_ctx->frameCrc = crc8_getForByte(byte, PROTO_CRC8_POLY, _ctx->frameCrc);
				}
				break;

			case RX_STATE_CRC:
				{
					if (_ctx->frameCrc != byte) {
						_response(PROTO_ERROR_INVALID_CRC, NULL, 0);

					} else {
						switch (_ctx->frameCmd) {
							case PROTO_CMD_GET_INFO:
								{
									uint8_t *b = ((uint8_t *) _ctx->params.memory);

									_ctx->frameDataSize = 0;

									b[_ctx->frameDataSize++] = PROTO_VERSION_MAJOR;
									b[_ctx->frameDataSize++] = PROTO_VERSION_MINOR;

									if (_ctx->params.memorySize > 127) {
										b[_ctx->frameDataSize++] = 0x80 | ((_ctx->params.memorySize & 0x7fff) >> 8);
									}

									b[_ctx->frameDataSize++] = _ctx->params.memorySize & 0xff;

									_response(PROTO_NO_ERROR, b, _ctx->frameDataSize);
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

		if (_ctx->rxState == RX_STATE_CHECK_PAYLOAD) {
			if (_ctx->frameDataSize > _ctx->params.memorySize - PROTO_FRAME_MIN_SIZE) {
				_response(PROTO_ERROR_INVALID_LENGTH, NULL, 0);

			} else {
				_ctx->rxState = RX_STATE_PAYLOAD;
			}
		}
	}
}


void proto_srv_onData(uint8_t *data, uint16_t dataSize) {
	while (dataSize) {
		programmer_onByte(*data);

		dataSize--;
		data++;
	}
}


void proto_srv_onIdle() {
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
