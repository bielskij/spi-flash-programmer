#include <gtest/gtest.h>

#include "common/protocol.h"
#include "common/protocol/packet.h"
#include "common/protocol/request.h"


struct RequestTestParameters {
	RequestTestParameters(
		uint8_t cmd, uint8_t id,
		std::function<void(ProtoReq &)> prepareReq,
		std::function<void(ProtoReq &)> fillReq,
		std::function<void(ProtoReq &)> validateReq
	) {
		this->cmd         = cmd;
		this->id          = id;
		this->prepareReq  = prepareReq;
		this->fillReq     = fillReq;
		this->validateReq = validateReq;
	}

	uint8_t cmd;
	uint8_t id;

	std::function<void(ProtoReq &)> prepareReq;
	std::function<void(ProtoReq &)> fillReq;
	std::function<void(ProtoReq &)> validateReq;
};


class RequestTestWithParameter : public testing::TestWithParam<RequestTestParameters> {
};


TEST_P(RequestTestWithParameter, common_protocol_request) {
	uint8_t  txBuffer[1024];
	uint16_t txBufferWritten;
	uint8_t  rxBuffer[1024];

	ProtoPkt pkt;

	{
		ProtoReq req;

		proto_req_init(&req, GetParam().cmd, GetParam().id);

		std::cout << "Testing command: " + std::to_string(GetParam().cmd) << std::endl;

		if (GetParam().prepareReq) {
			GetParam().prepareReq(req);
		}

		ASSERT_TRUE(proto_pkt_init(&pkt, txBuffer, sizeof(txBuffer), req.cmd, req.id, proto_req_getPayloadSize(&req)));
		{
			proto_req_assign(&req, pkt.payload, pkt.payloadSize);
			{
				if (GetParam().fillReq) {
					GetParam().fillReq(req);
				}
			}
			ASSERT_EQ(proto_req_encode(&req, pkt.payload, pkt.payloadSize), pkt.payloadSize);

			txBufferWritten = proto_pkt_encode(&pkt);
		}

		ASSERT_GT(txBufferWritten, 0);
	}

	{
		ProtoPktDes decoder;

		proto_pkt_dec_setup(&decoder, rxBuffer, sizeof(rxBuffer));

		uint16_t i;
		for (i = 0; i < txBufferWritten; i++) {
			auto ret = proto_pkt_dec_putByte(&decoder, txBuffer[i], &pkt);

			if (ret != PROTO_PKT_DES_RET_IDLE) {
				ProtoReq req;

				ASSERT_EQ(PROTO_PKT_DES_RET_GET_ERROR_CODE(ret), PROTO_NO_ERROR);

				ASSERT_EQ(pkt.code, GetParam().cmd);
				ASSERT_EQ(pkt.id,   GetParam().id);

				proto_req_init(&req, pkt.code, pkt.id);

				if (proto_req_getPayloadSize(&req)) {
					ASSERT_NE(pkt.payload,     nullptr);
					ASSERT_NE(pkt.payloadSize, 0);

					ASSERT_NE(proto_req_decode(&req, pkt.payload, pkt.payloadSize), 0);

				} else {
					ASSERT_EQ(pkt.payload,     nullptr);
					ASSERT_EQ(pkt.payloadSize, 0);

					ASSERT_EQ(proto_req_decode(&req, pkt.payload, pkt.payloadSize), 0);
				}

				proto_req_assign(&req, pkt.payload, pkt.payloadSize);

				ASSERT_EQ(req.cmd, GetParam().cmd);
				ASSERT_EQ(req.id,  GetParam().id);

				if (GetParam().validateReq) {
					GetParam().validateReq(req);
				}
				break;
			}
		}

		ASSERT_NE(i, txBufferWritten);
	}
}


INSTANTIATE_TEST_SUITE_P(common_protocol_request, RequestTestWithParameter, testing::Values(
	RequestTestParameters(
		PROTO_CMD_GET_INFO, 0x45,

		{},
		{},
		{}
	),

	RequestTestParameters(
		PROTO_CMD_SPI_TRANSFER, 0x45,

		[](ProtoReq &req) {
			auto &t = req.request.transfer;

			t.flags = 0xa5;

			t.txBufferSize = 10;
			t.rxBufferSize = 12;
			t.rxSkipSize   = 2;
		},

		[](ProtoReq &req) {
			auto &t = req.request.transfer;

			t.txBuffer[0] = 0x10;
			t.txBuffer[1] = 0x20;
		},

		[](ProtoReq &req) {
			auto &t = req.request.transfer;

			ASSERT_EQ(t.flags,        0xa5);
			ASSERT_EQ(t.txBufferSize,   10);
			ASSERT_EQ(t.rxBufferSize,   12);
			ASSERT_EQ(t.rxSkipSize,      2);

			ASSERT_TRUE(t.rxBuffer == NULL);
			ASSERT_TRUE(t.txBuffer != NULL);

			ASSERT_EQ(t.txBuffer[0], 0x10);
			ASSERT_EQ(t.txBuffer[1], 0x20);
		}
	)
));
