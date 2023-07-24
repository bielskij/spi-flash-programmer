#include <stdlib.h>
#include <string.h>

#include "common/protocol.h"
#include "common/protocol/packet.h"
#include "common/crc8.h"

#include "common.h"

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


uint16_t proto_pkt_enc(ProtoPkt *pkt, uint8_t *buffer, uint16_t bufferSize) {
	uint16_t ret = 0;

	do {
		if (bufferSize < 3 + proto_int_val_length_estimate(pkt->payloadSize) + pkt->payloadSize) {
			break;
		}

		buffer[ret++] = PROTO_SYNC_NIBBLE | pkt->code;
		buffer[ret++] = pkt->id;

		ret += proto_int_val_encode(pkt->payloadSize, buffer + ret);

		for (uint16_t i = 0; i < pkt->payloadSize; i++) {
			buffer[ret++] = ((uint8_t *)pkt->payload)[i];
		}

		buffer[ret++] = crc8_get(buffer, ret, PROTO_CRC8_POLY, PROTO_CRC8_START);
	} while (0);

	return ret;
}


void proto_pkt_dec_setup(ProtoPktDes *ctx, uint8_t *buffer, uint16_t bufferSize) {
	ctx->mem     = buffer;
	ctx->memSize = bufferSize;

	proto_pkt_dec_reset(ctx);
}


uint8_t proto_pkt_dec_putByte(ProtoPktDes *ctx, uint8_t byte, ProtoPkt *request) {
	uint8_t error = PROTO_NO_ERROR;

	switch (ctx->state) {
		case STATE_WAIT_SYNC:
			{
				if ((byte & PROTO_SYNC_NIBBLE_MASK) == PROTO_SYNC_NIBBLE) {
					ctx->code   = PROTO_CMD_NIBBLE_MASK & byte;
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
					error = PROTO_ERROR_INVALID_CRC;

				} else {
					request->code = ctx->code;
					request->id   = ctx->id;

					request->payloadSize = ctx->dataSize;
					if (request->payloadSize) {
						request->payload = ctx->mem;

					} else {
						request->payload = NULL;
					}

					ctx->state = STATE_CMD_RDY;
				}
			}
			break;
	}

	if (ctx->state == STATE_CHECK_PAYLOAD) {
		if (ctx->dataSize > ctx->memSize) {
			error = PROTO_ERROR_INVALID_LENGTH;

		} else {
			ctx->state = STATE_PAYLOAD;
		}
	}

	{
		uint8_t ret = PROTO_PKT_DES_RET_IDLE;

		if (error != PROTO_NO_ERROR) {
			ret = PROTO_PKT_DES_RET_SET_ERROR_CODE(error);

		} else if (ctx->state == STATE_CMD_RDY) {
			ret = PROTO_PKT_DES_RET_SET_ERROR_CODE(error);
		}

		if (ret != PROTO_PKT_DES_RET_IDLE) {
			proto_pkt_dec_reset(ctx);
		}

		return ret;
	}
}


void proto_pkt_dec_reset(ProtoPktDes *ctx) {
	ctx->state    = STATE_WAIT_SYNC;
	ctx->crc      = PROTO_CRC8_START;
	ctx->id       = ID_UNKNOWN;
	ctx->code     = CMD_UNKNOWN;
	ctx->dataRead = 0;
	ctx->dataSize = 0;
}
