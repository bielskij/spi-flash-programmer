#include <gtest/gtest.h>

#include "common/protocol.h"
#include "flashutil/debug.h"
#include "firmware/programmer.h"


static void _deserializeResponse(uint8_t *buffer, uint16_t bufferSize, uint8_t cmd, ProtoRes &res) {
	ProtoPktDes des;

	proto_pkt_dec_setup(&des, buffer, bufferSize);

	for (uint16_t i = 0; i < bufferSize; i++) {
		ProtoPkt pkt;

		auto ret = proto_pkt_dec_putByte(&des, buffer[i], &pkt);
		if (ret != PROTO_PKT_DES_RET_IDLE) {
			proto_res_init(&res, pkt.payload, pkt.payloadSize, cmd, PROTO_PKT_DES_RET_GET_ERROR_CODE(ret));

			proto_res_decode(&res, pkt.payload, pkt.payloadSize);
			proto_res_assign(&res, pkt.payload, pkt.payloadSize);
		}
	}
}


static void _requestGetInfoCallback(ProtoReq *request, ProtoRes *response, void *callbackData) {
	uint8_t *data = (uint8_t *) callbackData;

	ASSERT_TRUE(data != NULL);

	ASSERT_EQ(request->cmd, PROTO_CMD_GET_INFO);

	*data |= (1 << 0);
}


static void _responseGetInfoCallback(uint8_t *buffer, uint16_t bufferSize, void *callbackData) {
	uint8_t *data = (uint8_t *) callbackData;

	ASSERT_TRUE(data != NULL);

	{
		ProtoRes res;

		_deserializeResponse(buffer, bufferSize, PROTO_CMD_GET_INFO, res);

		ASSERT_EQ(res.cmd,  PROTO_CMD_GET_INFO);
		ASSERT_EQ(res.code, PROTO_NO_ERROR);

		ASSERT_EQ(res.response.getInfo.version.major, PROTO_VERSION_MAJOR);
		ASSERT_EQ(res.response.getInfo.version.minor, PROTO_VERSION_MINOR);
		ASSERT_GT(res.response.getInfo.packetSize,   0);
	}

	*data |= (1 << 1);
}


TEST(firmware_programmer, proto_getInfo) {
	std::vector<uint8_t> buffer(1024, 0);

	{
		Programmer prog;

		uint8_t callbackData = 0;

		programmer_setup(
			&prog, buffer.data(), buffer.size(), _requestGetInfoCallback, _responseGetInfoCallback, &callbackData
		);

		{
			std::vector<uint8_t> reqBuffer(1024, 0);
			uint16_t             reqWritten;

			{
				ProtoPkt pkt;

				proto_pkt_init(&pkt, reqBuffer.data(), reqBuffer.size(), PROTO_CMD_GET_INFO, 0x05);

				{
					ProtoReq req;

					proto_req_init  (&req, pkt.payload, pkt.payloadSize, pkt.code);
					proto_req_assign(&req, pkt.payload, pkt.payloadSize);
					proto_req_encode(&req, pkt.payload, pkt.payloadSize);

					ASSERT_TRUE(proto_pkt_prepare(&pkt, reqBuffer.data(), reqBuffer.size(), proto_req_getPayloadSize(&req)));
				}

				reqWritten = proto_pkt_encode(&pkt, reqBuffer.data(), reqBuffer.size());
			}

			for (int i = 0; i < reqWritten; i++) {
				programmer_putByte(&prog, reqBuffer[i]);
			}
		}

		ASSERT_EQ(callbackData, 0x03);
	}
}


static void _requestTransferCallback(ProtoReq *request, ProtoRes *response, void *callbackData) {
	uint8_t *data = (uint8_t *) callbackData;

	ASSERT_TRUE(data != NULL);

	ASSERT_EQ(request->cmd, PROTO_CMD_SPI_TRANSFER);

	{
		ProtoReqTransfer &t = request->request.transfer;

		ASSERT_EQ(t.flags,        PROTO_SPI_TRANSFER_FLAG_KEEP_CS);
		ASSERT_EQ(t.rxBufferSize, 128);
		ASSERT_EQ(t.rxSkipSize,   32);
		ASSERT_EQ(t.txBufferSize, 32);

		ASSERT_TRUE(t.txBuffer != NULL);

		for (uint16_t i = 0; i < t.txBufferSize; i++) {
			ASSERT_EQ(t.txBuffer[i], i);
		}
	}

	{
		ProtoResTransfer &t = response->response.transfer;

		for (uint16_t i = 0; i < t.rxBufferSize; i++) {
			t.rxBuffer[i] = 255 - i;
		}
	}

	*data |= (1 << 0);
}


static void _responseTransferCallback(uint8_t *buffer, uint16_t bufferSize, void *callbackData) {
	uint8_t *data = (uint8_t *) callbackData;

	ASSERT_TRUE(data != NULL);

	{
		ProtoRes res;

		_deserializeResponse(buffer, bufferSize, PROTO_CMD_SPI_TRANSFER, res);

		ASSERT_EQ(res.cmd,  PROTO_CMD_SPI_TRANSFER);
		ASSERT_EQ(res.code, PROTO_NO_ERROR);

		{
			ProtoResTransfer &t = res.response.transfer;

			ASSERT_EQ(t.rxBufferSize, 128);
			ASSERT_TRUE(t.rxBuffer != NULL);

			for (uint16_t i = 0; i < t.rxBufferSize; i++) {
				ASSERT_EQ(t.rxBuffer[i], 255 - i);
			}
		}
	}

	*data |= (1 << 1);
}


TEST(firmware_programmer, proto_transfer) {
	std::vector<uint8_t> buffer(1024, 0);

	{
		Programmer prog;

		uint8_t callbackData = 0;

		programmer_setup(
			&prog, buffer.data(), buffer.size(), _requestTransferCallback, _responseTransferCallback, &callbackData
		);

		{
			std::vector<uint8_t> reqBuffer(1024, 0);
			uint16_t             reqWritten;

			{
				ProtoPkt pkt;

				proto_pkt_init(&pkt, reqBuffer.data(), reqBuffer.size(), PROTO_CMD_SPI_TRANSFER, 0x5);

				{
					ProtoReq req;

					proto_req_init(&req, pkt.payload, pkt.payloadSize, pkt.code);

					req.request.transfer.rxBufferSize = 128;
					req.request.transfer.txBufferSize = 32;
					req.request.transfer.rxSkipSize   = 32;
					req.request.transfer.flags        = PROTO_SPI_TRANSFER_FLAG_KEEP_CS;

					proto_pkt_prepare(&pkt, reqBuffer.data(), reqBuffer.size(), proto_req_getPayloadSize(&req));

					proto_req_assign(&req, pkt.payload, pkt.payloadSize);
					{
						ASSERT_TRUE(req.request.transfer.txBuffer != NULL);

						for (uint16_t i = 0; i < req.request.transfer.txBufferSize; i++) {
							req.request.transfer.txBuffer[i] = i;
						}
					}
					proto_req_encode(&req, pkt.payload, pkt.payloadSize);
				}

				reqWritten = proto_pkt_encode(&pkt, reqBuffer.data(), reqBuffer.size());

				debug_dumpBuffer(reqBuffer.data(), reqWritten, 32, 0);
			}

			for (int i = 0; i < reqWritten; i++) {
				programmer_putByte(&prog, reqBuffer[i]);
			}
		}

		ASSERT_EQ(callbackData, 0x03);
	}
}
