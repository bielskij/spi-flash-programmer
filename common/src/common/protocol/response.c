#include <stdlib.h>

#include "common/protocol.h"
#include "common/protocol/response.h"

#include "common.h"


bool proto_res_dec(ProtoRes *request, ProtoPkt *pkt) {
	bool ret = true;

	{
		uint8_t *payload = (uint8_t *) pkt->payload;

		request->cmd = pkt->code;
		request->id  = pkt->id;

		switch (request->cmd) {
			case PROTO_CMD_GET_INFO:
				{
					ProtoResGetInfo *info = &request->response.getInfo;

					if (pkt->payloadSize < 3) {
						ret = false;
						break;
					}

					info->version.major = payload[0] >> 4;
					info->version.minor = payload[0] & 0x0f;
					info->payloadSize   = proto_int_val_decode(payload + 1);
				}
				break;

			default:
				break;
		}
	}

	return ret;
}


bool proto_res_enc(ProtoRes *request, ProtoPkt *pkt, uint8_t *buffer, uint16_t bufferSize) {
	bool ret = true;

	{
		uint16_t ret = 0;

		pkt->code = request->cmd;
		pkt->id   = request->id;

		switch (request->cmd) {
			case PROTO_CMD_GET_INFO:
				{
					if (bufferSize < 3) {
						ret = false;
						break;
					}

					buffer[ret++] = (request->response.getInfo.version.major << 4) | request->response.getInfo.version.minor;

					ret += proto_int_val_encode(request->response.getInfo.payloadSize, buffer + ret);
				}
				break;

			default:
				break;
		}

		if (ret) {
			pkt->payload     = buffer;
			pkt->payloadSize = ret;

		} else {
			pkt->payload     = NULL;
			pkt->payloadSize = 0;
		}
	}

	return ret;
}
