/*
 * request.c
 *
 *  Created on: 23 lip 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */
#include <stdlib.h>

#include "common/protocol.h"
#include "common/protocol/deserializer/request.h"


#define CRC_UNKNOWN 0
#define CMD_UNKNOWN 0
#define ID_UNKNOWN  0


typedef enum _State {
	STATE_WAIT_SYNC,
	STATE_ID,
	STATE_VLEN_HI,
	STATE_VLEN_LO,
	STATE_CHECK_PAYLOAD,
	STATE_PAYLOAD,
	STATE_CRC,
	STATE_CMD_RDY
} State;


uint8_t proto_int_val_length_estimate(uint16_t val) {
	if (val > 127) {
		return 2;
	}

	return 1;
}


uint8_t proto_int_val_length_probe(uint8_t byte) {
	if (byte & 0x80) {
		return 2;
	}

	return 1;
}


uint16_t proto_int_val_decode(uint8_t val[2]) {
	return ((uint16_t)(val[0] & 0x7f) << 8) | val[1];
}


void PRQD_setup(PRQDContext *ctx, void *mem, uint16_t memSize) {
	ctx->mem     = mem;
	ctx->memSize = memSize;

	PRQD_reset(ctx);
}


uint8_t PRQD_putByte(PRQDContext *ctx, uint8_t byte, PRQ *request) {
	uint8_t error = PROTO_NO_ERROR;

	switch (ctx->state) {
		case STATE_WAIT_SYNC:
			{
				if ((byte & PROTO_SYNC_NIBBLE_MASK) == PROTO_SYNC_NIBBLE) {
					ctx->cmd   = PROTO_CMD_NIBBLE_MASK & byte;
					ctx->state = STATE_ID;
					ctx->crc   = crc8_getForByte(byte, PROTO_CRC8_POLY, PROTO_CRC8_START);
				}
			}
			break;

		case STATE_ID:
			{
				ctx->id    = byte;
				ctx->state = STATE_VLEN_HI;
				ctx->crc   = crc8_getForByte(byte, PROTO_CRC8_POLY, ctx->crc);
			}
			break;

		case STATE_VLEN_HI:
			{
				if (proto_int_val_length_probe(byte) == 1) {
					ctx->dataSize = byte;

					if (ctx->dataSize == 0) {
						ctx->state = STATE_CRC;

					} else {
						ctx->state = STATE_CHECK_PAYLOAD;
					}

				} else {
					ctx->mem[0] = byte;

					ctx->state = STATE_VLEN_LO;
				}

				ctx->crc = crc8_getForByte(byte, PROTO_CRC8_POLY, ctx->crc);
			}
			break;

		case STATE_VLEN_LO:
			{
				ctx->mem[1] = byte;

				ctx->dataSize = proto_int_val_decode(ctx->mem);

				ctx->state = STATE_CHECK_PAYLOAD;

				ctx->crc = crc8_getForByte(byte, PROTO_CRC8_POLY, ctx->crc);
			}
			break;

		case STATE_PAYLOAD:
			{
				((uint8_t *) ctx->mem)[ctx->dataRead++] = byte;

				if (ctx->dataRead == ctx->dataSize) {
					ctx->state = STATE_CRC;
				}

				ctx->crc = crc8_getForByte(byte, PROTO_CRC8_POLY, ctx->crc);
			}
			break;

		case STATE_CRC:
			{
				if (ctx->crc != byte) {
					_response(PROTO_ERROR_INVALID_CRC, NULL, 0);

				} else {
					ctx->state = STATE_CMD_RDY;

					switch (ctx->cmd) {
						case PROTO_CMD_GET_INFO:
							{
								request->cmd = ctx->cmd;
							}
							break;

						default:
							{
								error = PROTO_ERROR_INVALID_CMD;
							}
							break;
					}
				}
			}
			break;
	}

	if (ctx->state == STATE_CHECK_PAYLOAD) {
		if (ctx->dataSize > ctx->memSize - PROTO_FRAME_MIN_SIZE) {
			error = PROTO_ERROR_INVALID_LENGTH;

		} else {
			ctx->state = STATE_PAYLOAD;
		}
	}

	{
		uint8_t ret = PRQD_RET_IDLE;

		if (error != PROTO_NO_ERROR) {
			request->cmd = PROTO_CMD_ERROR;
			request->id  = ctx->id;

			request->request.error.code = error;

			ret = PRQD_RET_SET_ERROR_CODE(error);

		} else if (ctx->state == STATE_CMD_RDY) {
			ret = PRQD_RET_SET_ERROR_CODE(error);
		}

		if (ret != PRQD_RET_IDLE) {
			PRQD_reset(ctx);
		}

		return ret;
	}
}


void PRQD_reset(PRQDContext *ctx) {
	ctx->state    = STATE_WAIT_SYNC;
	ctx->crc      = CRC_UNKNOWN;
	ctx->id       = ID_UNKNOWN;
	ctx->cmd      = CMD_UNKNOWN;
	ctx->dataRead = 0;
	ctx->dataSize = 0;
}
