#include <stdlib.h>

#include "common/protocol.h"
#include "common/protocol/request.h"


bool proto_req_dec(ProtoReq *request, ProtoPkt *pkt) {
	bool ret = true;

	{
		request->cmd = pkt->code;
		request->id  = pkt->id;

		switch (request->cmd) {
			case PROTO_CMD_GET_INFO:
				{
				}
				break;

			default:
				ret = false;
		}
	}

	return ret;
}


bool proto_req_enc(ProtoReq *request, ProtoPkt *pkt, uint8_t *buffer, uint16_t bufferSize) {
	bool ret = true;

	{
		uint16_t ret = 0;

		pkt->code = request->cmd;
		pkt->id   = request->id;

		switch (request->cmd) {
			case PROTO_CMD_GET_INFO:
				{
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
