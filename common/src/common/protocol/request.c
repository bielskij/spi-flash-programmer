#include <stdlib.h>

#include "common/protocol.h"
#include "common/protocol/request.h"

#include "common.h"


void proto_req_init(ProtoReq *request, uint8_t cmd, uint8_t id) {
	request->cmd = cmd;
	request->id  = id;
}


uint16_t proto_req_getPayloadSize(ProtoReq *request) {
	uint16_t ret = 0;

	switch (request->cmd) {
		case PROTO_CMD_GET_INFO:
			{
			}
			break;

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


void proto_req_assign(ProtoReq *request, uint8_t *memory, uint16_t memorySize) {
	switch (request->cmd) {
		case PROTO_CMD_GET_INFO:
			{
			}
			break;

		case PROTO_CMD_SPI_TRANSFER:
			{
				ProtoReqTransfer *t = &request->request.transfer;

				if (t->txBufferSize) {
					t->txBuffer = memory + (1 + proto_int_val_length_estimate(t->txBufferSize));

				} else {
					t->txBuffer = NULL;
				}
			}
			break;

		default:
			{

			}
			break;
	}
}


uint16_t proto_req_encode(ProtoReq *request, uint8_t *buffer, uint16_t bufferSize) {
	uint16_t ret = 0;

	switch (request->cmd) {
		case PROTO_CMD_GET_INFO:
			{
			}
			break;

		case PROTO_CMD_SPI_TRANSFER:
			{
				ProtoReqTransfer *t = &request->request.transfer;

				buffer[ret++] = t->flags;

				ret += proto_int_val_encode(t->txBufferSize, buffer + ret);

				ret += t->txBufferSize;

				ret += proto_int_val_encode(t->rxSkipSize,   buffer + ret);
				ret += proto_int_val_encode(t->rxBufferSize, buffer + ret);
			}
			break;

		default:
			{

			}
			break;
	}

	return ret;
}


uint16_t proto_req_decode(ProtoReq *request, uint8_t *buffer, uint16_t bufferSize) {
	uint16_t ret = 0;

	switch (request->cmd) {
		case PROTO_CMD_GET_INFO:
			{
			}
			break;

		case PROTO_CMD_SPI_TRANSFER:
			{
				ProtoReqTransfer *t = &request->request.transfer;

				t->flags        = buffer[ret++];
				t->txBuffer     = NULL;
				t->txBufferSize = proto_int_val_decode(buffer + ret); ret += proto_int_val_length_estimate(t->txBufferSize);

				ret += t->txBufferSize;

				t->rxSkipSize   = proto_int_val_decode(buffer + ret); ret += proto_int_val_length_estimate(t->rxSkipSize);;
				t->rxBufferSize = proto_int_val_decode(buffer + ret); ret += proto_int_val_length_estimate(t->rxBufferSize);
			}
			break;

		default:
			break;
	}

	return ret;
}
