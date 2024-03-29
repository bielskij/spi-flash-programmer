#include <stdlib.h>

#include "common/protocol.h"
#include "common/protocol/response.h"

#include "common.h"


#define PTR_U8(x) ((uint8_t *)(x))


void proto_res_init(ProtoRes *response, void *memory, uint16_t memorySize, uint8_t cmd) {
	uint8_t overHead = 1;

	response->cmd = cmd;

	switch (cmd) {
		case PROTO_CMD_SPI_TRANSFER:
			{
				ProtoResTransfer *t = &response->response.transfer;

				if (memorySize > overHead) {
					t->rxBufferSize = memorySize - overHead;
				}

				t->rxBuffer = NULL;
			}
			break;

		default:
			break;
	}
}


uint16_t proto_res_getPayloadSize(ProtoRes *response) {
	uint16_t ret = 0;

	switch (response->cmd) {
		case PROTO_CMD_GET_INFO:
			{
				ret = 1 + proto_int_val_length_estimate(response->response.getInfo.packetSize);
			}
			break;

		case PROTO_CMD_SPI_TRANSFER:
			{
				ProtoResTransfer *t = &response->response.transfer;

				ret = t->rxBufferSize;
			}
			break;

		default:
			break;
	}

	return ret;
}


void proto_res_assign(ProtoRes *response, void *memory, uint16_t memorySize) {
	switch (response->cmd) {
		case PROTO_CMD_GET_INFO:
			{
			}
			break;

		case PROTO_CMD_SPI_TRANSFER:
			{
				ProtoResTransfer *t = &response->response.transfer;

				if (t->rxBufferSize) {
					t->rxBuffer = PTR_U8(memory);

				} else {
					t->rxBuffer = NULL;
				}
			}
			break;

		default:
			{

			}
			break;
	}
}


uint16_t proto_res_encode(ProtoRes *response, void *memory, uint16_t memorySize) {
	uint16_t ret = 0;

	switch (response->cmd) {
		case PROTO_CMD_GET_INFO:
			{
				ProtoResGetInfo *info = &response->response.getInfo;

				PTR_U8(memory)[ret++] = (info->version.major << 4) | (info->version.minor & 0x0f);

				ret += proto_int_val_encode(info->packetSize, PTR_U8(memory) + ret);
			}
			break;

		case PROTO_CMD_SPI_TRANSFER:
			{
				ProtoResTransfer *t = &response->response.transfer;

				ret += t->rxBufferSize;
			}
			break;

		default:
			break;
	}

	return ret;
}


uint16_t proto_res_decode(ProtoRes *response, void *memory, uint16_t memorySize) {
	uint16_t ret = 0;

	switch (response->cmd) {
		case PROTO_CMD_GET_INFO:
			{
				ProtoResGetInfo *info = &response->response.getInfo;

				info->version.major = PTR_U8(memory)[ret] >> 4;
				info->version.minor = PTR_U8(memory)[ret++] & 0x0f;
				info->packetSize   = proto_int_val_decode(PTR_U8(memory) + ret);

				ret += proto_int_val_length_estimate(info->packetSize);
			}
			break;

		case PROTO_CMD_SPI_TRANSFER:
			{
				ProtoResTransfer *t = &response->response.transfer;

				t->rxBuffer     = NULL;
				t->rxBufferSize = memorySize;

				ret += t->rxBufferSize;
			}
			break;

		default:
			break;
	}

	return ret;
}
