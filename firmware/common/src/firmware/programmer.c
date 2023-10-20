#include "common/protocol.h"

#include "firmware/programmer.h"


void programmer_setup(
	Programmer                *programmer,
	uint8_t                   *memory,
	uint16_t                   memorySize,
	ProgrammerRequestCallback  requestCallback,
	ProgrammerResponseCallback responseCallback,
	void                      *callbackData
) {
	programmer->mem     = memory;
	programmer->memSize = memorySize;

	programmer->requestCallback  = requestCallback;
	programmer->responseCallback = responseCallback;
	programmer->callbackData     = callbackData;

	proto_pkt_dec_setup(
		&programmer->packetDeserializer,
		programmer->mem,
		programmer->memSize
	);
}


static void _sendError(Programmer *programmer, ProtoPkt *packet, ProtoRes *response, uint8_t errorCode) {
	proto_pkt_init(packet, programmer->mem, programmer->memSize, packet->code, packet->id);
	proto_res_init(response, packet->payload, packet->payloadSize, packet->code, errorCode);

	proto_pkt_prepare(packet, programmer->mem, programmer->memSize, proto_res_getPayloadSize(response));

	proto_res_assign(response, packet->payload, packet->payloadSize);
	proto_res_encode(response, packet->payload, packet->payloadSize);
}


void programmer_putByte(Programmer *programmer, uint8_t byte) {
	ProtoPkt packet;

	uint8_t ret = proto_pkt_dec_putByte(&programmer->packetDeserializer, byte, &packet);

	if (ret != PROTO_PKT_DES_RET_IDLE) {
		ProtoRes response;

		do {
			ProtoReq request;

			uint16_t packetMaxSize;

			if (PROTO_PKT_DES_RET_GET_ERROR_CODE(ret) != PROTO_NO_ERROR) {
				_sendError(programmer, &packet, &response, PROTO_PKT_DES_RET_GET_ERROR_CODE(ret));
				break;
			}

			// Parse, assign request to coming packet
			proto_req_init  (&request, packet.payload, packet.payloadSize, packet.code);
			proto_req_decode(&request, packet.payload, packet.payloadSize);
			proto_req_assign(&request, packet.payload, packet.payloadSize);

			// Start preparation of response
			proto_pkt_init(&packet, programmer->mem, programmer->memSize, packet.code, packet.id);
			proto_res_init(&response, packet.payload, packet.payloadSize, packet.code, PROTO_NO_ERROR);

			packetMaxSize = packet.payloadSize;

			switch (request.cmd) {
				case PROTO_CMD_GET_INFO:
					{
						ProtoResGetInfo *res = &response.response.getInfo;

						res->version.major = PROTO_VERSION_MAJOR;
						res->version.minor = PROTO_VERSION_MINOR;

						res->packetSize = packetMaxSize;
					}
					break;

				case PROTO_CMD_SPI_TRANSFER:
					{
						ProtoResTransfer *res = &response.response.transfer;

						if (res->rxBufferSize < request.request.transfer.rxBufferSize) {
							_sendError(programmer, &packet, &response, PROTO_ERROR_INVALID_MESSAGE);

						} else {
							res->rxBufferSize = request.request.transfer.rxBufferSize;
						}
					}
					break;

				default:
					_sendError(programmer, &packet, &response, PROTO_ERROR_INVALID_CMD);
					break;
			}

			if (response.code != PROTO_NO_ERROR) {
				break;
			}

			proto_pkt_prepare(&packet, programmer->mem, programmer->memSize, proto_res_getPayloadSize(&response));

			proto_res_assign(&response, packet.payload, packet.payloadSize);
			{
				programmer->requestCallback(&request, &response, programmer->callbackData);
			}
			proto_res_encode(&response, packet.payload, packet.payloadSize);

		} while (0);

		programmer->responseCallback(
			programmer->mem, proto_pkt_encode(&packet, programmer->mem, programmer->memSize), programmer->callbackData
		);
	}
}


void programmer_reset(Programmer *programmer) {
	proto_pkt_dec_reset(&programmer->packetDeserializer);
}
