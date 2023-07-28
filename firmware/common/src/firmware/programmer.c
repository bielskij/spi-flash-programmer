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


void programmer_putByte(Programmer *programmer, uint8_t byte) {
	ProtoPkt packet;

	uint8_t ret = proto_pkt_dec_putByte(&programmer->packetDeserializer, byte, &packet);

	if (ret != PROTO_PKT_DES_RET_IDLE) {
		ProtoRes response;

		if (PROTO_PKT_DES_RET_GET_ERROR_CODE(ret) != PROTO_NO_ERROR) {
			proto_res_init(&response, packet.code, PROTO_PKT_DES_RET_GET_ERROR_CODE(ret), packet.id);

			proto_pkt_init(
				&packet,
				programmer->mem,
				programmer->memSize,
				response.code,
				response.id,
				proto_res_getPayloadSize(&response)
			);

			proto_res_assign(&response, packet.payload, packet.payloadSize);
			proto_res_encode(&response, packet.payload, packet.payloadSize);

		} else {
			ProtoReq request;

			proto_req_init  (&request, packet.code,    packet.id);
			proto_req_decode(&request, packet.payload, packet.payloadSize);
			proto_req_assign(&request, packet.payload, packet.payloadSize);

			proto_res_init(&response, request.cmd, PROTO_NO_ERROR, request.id);

			switch (request.cmd) {
				case PROTO_CMD_GET_INFO:
					{
						ProtoResGetInfo *res = &response.response.getInfo;

						res->version.major = PROTO_VERSION_MAJOR;
						res->version.minor = PROTO_VERSION_MINOR;

						res->payloadSize = programmer->memSize - PROTO_FRAME_MIN_SIZE;
					}
					break;

				case PROTO_CMD_SPI_TRANSFER:
					{
						ProtoResTransfer *res = &response.response.transfer;

						res->rxBufferSize = request.request.transfer.rxBufferSize;
					}
					break;

				default:
					proto_res_init(&response, request.cmd, PROTO_ERROR_INVALID_CMD, request.id);
					break;
			}

			proto_pkt_init(
				&packet,
				programmer->mem,
				programmer->memSize,
				response.code,
				response.id,
				proto_res_getPayloadSize(&response)
			);

			proto_res_assign(&response, packet.payload, packet.payloadSize);
			{
				if (response.code == PROTO_NO_ERROR) {
					programmer->requestCallback(&request, &response, programmer->callbackData);

					if (response.code != PROTO_NO_ERROR) {
						proto_pkt_init(
							&packet,
							programmer->mem,
							programmer->memSize,
							response.code,
							response.id,
							proto_res_getPayloadSize(&response)
						);
					}
				}
			}
			proto_res_encode(&response, packet.payload, packet.payloadSize);

			programmer->responseCallback(
				programmer->mem, proto_pkt_encode(&packet), programmer->callbackData
			);
		}
	}
}


void programmer_reset(Programmer *programmer) {
	proto_pkt_dec_reset(&programmer->packetDeserializer);
}
