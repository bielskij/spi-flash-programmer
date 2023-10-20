#include <stdlib.h>

#include "common/protocol.h"
#include "common/protocol/request.h"

#include "common.h"


#define PTR_U8(x) ((uint8_t *)(x))


void proto_req_init(ProtoReq *request, void *memory, uint16_t memorySize, uint8_t cmd) {
	request->cmd = cmd;

	switch (request->cmd) {
		case PROTO_CMD_SPI_TRANSFER:
			{
				ProtoReqTransfer *t = &request->request.transfer;

				uint8_t overhead = 7;

				if (memorySize > 7) {
					t->txBufferSize = memorySize - overhead;

				} else {
					t->txBufferSize = 0;
				}

				t->txBuffer = NULL;
			}
			break;

		default:
			break;
	}
}


uint16_t proto_req_getPayloadSize(ProtoReq *request) {
	uint16_t ret = 0;

	switch (request->cmd) {
		case PROTO_CMD_SPI_TRANSFER:
			{
				ProtoReqTransfer *t = &request->request.transfer;

				ret =
					1 +
					proto_int_val_length_estimate(t->txBufferSize) +
					t->txBufferSize +
					proto_int_val_length_estimate(t->rxBufferSize) +
					proto_int_val_length_estimate(t->rxSkipSize);
			}
			break;

		default:
			break;
	}

	return ret;
}


void proto_req_assign(ProtoReq *request, void *memory, uint16_t memorySize) {
	switch (request->cmd) {
		case PROTO_CMD_SPI_TRANSFER:
			{
				ProtoReqTransfer *t = &request->request.transfer;

				if (t->txBufferSize) {
					t->txBuffer = PTR_U8(memory) + (1 + proto_int_val_length_estimate(t->txBufferSize));

				} else {
					t->txBuffer = NULL;
				}
			}
			break;

		default:
			break;
	}
}


uint16_t proto_req_encode(ProtoReq *request, void *memory, uint16_t memorySize) {
	uint16_t ret = 0;

	switch (request->cmd) {
		case PROTO_CMD_GET_INFO:
			{
			}
			break;

		case PROTO_CMD_SPI_TRANSFER:
			{
				ProtoReqTransfer *t = &request->request.transfer;

				PTR_U8(memory)[ret++] = t->flags;

				ret += proto_int_val_encode(t->txBufferSize, PTR_U8(memory) + ret);

				ret += t->txBufferSize;

				ret += proto_int_val_encode(t->rxSkipSize,   PTR_U8(memory) + ret);
				ret += proto_int_val_encode(t->rxBufferSize, PTR_U8(memory) + ret);
			}
			break;

		default:
			{

			}
			break;
	}

	return ret;
}


uint16_t proto_req_decode(ProtoReq *request, void *memory, uint16_t memorySize) {
	uint16_t ret = 0;

	switch (request->cmd) {
		case PROTO_CMD_GET_INFO:
			{
			}
			break;

		case PROTO_CMD_SPI_TRANSFER:
			{
				ProtoReqTransfer *t = &request->request.transfer;

				t->flags        = PTR_U8(memory)[ret++];
				t->txBuffer     = NULL;
				t->txBufferSize = proto_int_val_decode(PTR_U8(memory) + ret); ret += proto_int_val_length_estimate(t->txBufferSize);

				ret += t->txBufferSize;

				t->rxSkipSize   = proto_int_val_decode(PTR_U8(memory) + ret); ret += proto_int_val_length_estimate(t->rxSkipSize);;
				t->rxBufferSize = proto_int_val_decode(PTR_U8(memory) + ret); ret += proto_int_val_length_estimate(t->rxBufferSize);
			}
			break;

		default:
			break;
	}

	return ret;
}
