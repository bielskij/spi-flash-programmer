#include <stdlib.h>

#include "common/protocol.h"
#include "common/protocol/response.h"

#include "common.h"


void proto_res_init(ProtoRes *response, uint8_t cmd, uint8_t id) {
	response->cmd = cmd;
	response->id  = id;
}


uint16_t proto_res_getPayloadSize(ProtoRes *response) {
	uint16_t ret = 0;

	switch (response->cmd) {
		case PROTO_CMD_GET_INFO:
			{
				ret = 1 + proto_int_val_length_estimate(response->response.getInfo.payloadSize);
			}
			break;

		case PROTO_CMD_SPI_TRANSFER:
			{
				ProtoResTransfer *t = &response->response.transfer;

				ret = proto_int_val_length_estimate(t->rxBufferSize) + t->rxBufferSize;
			}
			break;

		default:
			ret = false;
	}

	return ret;
}


void proto_res_assign(ProtoRes *response, uint8_t *memory, uint16_t memorySize, bool decode) {
	switch (response->cmd) {
		case PROTO_CMD_GET_INFO:
			{
			}
			break;

		case PROTO_CMD_SPI_TRANSFER:
			{
				ProtoResTransfer *t = &response->response.transfer;

				if (t->rxBufferSize) {
					t->rxBuffer = memory + proto_int_val_length_estimate(t->rxBufferSize);

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


uint16_t proto_res_encode(ProtoRes *response, uint8_t *buffer, uint16_t bufferSize) {
	uint16_t ret = 0;

	switch (response->cmd) {
		case PROTO_CMD_GET_INFO:
			{
				ProtoResGetInfo *info = &response->response.getInfo;

				buffer[ret++] = (info->version.major << 4) | (info->version.minor & 0x0f);

				ret += proto_int_val_encode(info->payloadSize, buffer + ret);
			}
			break;

		case PROTO_CMD_SPI_TRANSFER:
			{
				ProtoResTransfer *t = &response->response.transfer;

				ret += proto_int_val_encode(t->rxBufferSize, buffer + ret);
			}
			break;

		default:
			break;
	}

	return ret;
}


uint16_t proto_res_decode(ProtoRes *response, uint8_t *buffer, uint16_t bufferSize) {
	uint16_t ret = 0;

	switch (response->cmd) {
		case PROTO_CMD_GET_INFO:
			{
				ProtoResGetInfo *info = &response->response.getInfo;

				info->version.major = buffer[ret] >> 4;
				info->version.minor = buffer[ret++] & 0x0f;
				info->payloadSize   = proto_int_val_decode(buffer + ret);

				ret += proto_int_val_length_estimate(info->payloadSize);
			}
			break;

		case PROTO_CMD_SPI_TRANSFER:
			{
				ProtoResTransfer *t = &response->response.transfer;

				t->rxBuffer     = NULL;
				t->rxBufferSize = proto_int_val_decode(buffer);
			}
			break;

		default:
			break;
	}

	return ret;
}
