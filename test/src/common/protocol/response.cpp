#include <gtest/gtest.h>

#include "common/protocol.h"
#include "common/protocol/response.h"
#include "common/protocol/packet.h"


struct ResponseTestParameters {
	ResponseTestParameters(
		uint8_t cmd, uint8_t code, uint8_t id,
		std::function<void(ProtoRes &)> prepareRes,
		std::function<void(ProtoRes &)> fillRes,
		std::function<void(ProtoRes &)> validateRes
	) {
		this->cmd         = cmd;
		this->code        = code;
		this->id          = id;
		this->prepareRes  = prepareRes;
		this->fillRes     = fillRes;
		this->validateRes = validateRes;
	}

	uint8_t cmd;
	uint8_t code;
	uint8_t id;

	std::function<void(ProtoRes &)> prepareRes;
	std::function<void(ProtoRes &)> fillRes;
	std::function<void(ProtoRes &)> validateRes;
};


class ResponseTestWithParameter : public testing::TestWithParam<ResponseTestParameters> {
};


TEST_P(ResponseTestWithParameter, common_protocol_response) {
	std::vector<uint8_t> txBuffer(1024, 0);
	std::vector<uint8_t> rxBuffer(1024, 0);
	size_t               txBufferWritten;


	ProtoPkt pkt;

	proto_pkt_init(&pkt, txBuffer.data(), txBuffer.size(), GetParam().cmd, GetParam().id);

	{
		ProtoRes res;

		proto_res_init(&res, pkt.payload, pkt.payloadSize, pkt.code, GetParam().code);

		std::cout << "Testing command: " << std::to_string(GetParam().cmd) << ", code: " << std::to_string(GetParam().code) << std::endl;

		if (GetParam().prepareRes) {
			GetParam().prepareRes(res);
		}

		{
			proto_res_assign(&res, pkt.payload, pkt.payloadSize);
			{
				if (GetParam().fillRes) {
					GetParam().fillRes(res);
				}
			}
			ASSERT_EQ(proto_res_encode(&res, pkt.payload, pkt.payloadSize), pkt.payloadSize);

			ASSERT_TRUE(proto_pkt_prepare(&pkt, txBuffer.data(), txBuffer.size(), proto_res_getPayloadSize(&res)));

			txBufferWritten = proto_pkt_encode(&pkt, txBuffer.data(), txBuffer.size());
		}

		ASSERT_GT(txBufferWritten, 0);
	}

	{
		ProtoPktDes decoder;

		proto_pkt_dec_setup(&decoder, rxBuffer.data(), txBuffer.size());

		uint16_t i;
		for (i = 0; i < txBufferWritten; i++) {
			auto ret = proto_pkt_dec_putByte(&decoder, txBuffer[i], &pkt);

			if (ret != PROTO_PKT_DES_RET_IDLE) {
				ProtoRes res;

				ASSERT_EQ(PROTO_PKT_DES_RET_GET_ERROR_CODE(ret), PROTO_NO_ERROR);

				ASSERT_EQ(pkt.code, GetParam().code);
				ASSERT_EQ(pkt.id,   GetParam().id);

				proto_res_init(&res, pkt.payload, pkt.payloadSize, GetParam().cmd, pkt.code);

				if (proto_res_getPayloadSize(&res)) {
					ASSERT_NE(pkt.payload,     nullptr);
					ASSERT_NE(pkt.payloadSize, 0);

					ASSERT_NE(proto_res_decode(&res, pkt.payload, pkt.payloadSize), 0);

				} else {
					ASSERT_EQ(pkt.payload,     nullptr);
					ASSERT_EQ(pkt.payloadSize, 0);

					ASSERT_EQ(proto_res_decode(&res, pkt.payload, pkt.payloadSize), 0);
				}

				proto_res_assign(&res, pkt.payload, pkt.payloadSize);

				ASSERT_EQ(res.cmd, GetParam().cmd);
				ASSERT_EQ(pkt.id,  GetParam().id);

				if (GetParam().validateRes) {
					GetParam().validateRes(res);
				}
				break;
			}
		}

		ASSERT_NE(i, txBufferWritten);
	}
}


INSTANTIATE_TEST_SUITE_P(common_protocol_response, ResponseTestWithParameter, testing::Values(
	ResponseTestParameters(
		PROTO_CMD_GET_INFO, PROTO_NO_ERROR, 0x45,

		{},
		{},
		{}
	),

	ResponseTestParameters(
		PROTO_CMD_SPI_TRANSFER, PROTO_NO_ERROR, 0x45,

		[](ProtoRes &res) {
			auto &t = res.response.transfer;

			t.rxBufferSize = 12;
		},

		[](ProtoRes &res) {
			auto &t = res.response.transfer;

			t.rxBuffer[0] = 0x10;
			t.rxBuffer[1] = 0x20;
		},

		[](ProtoRes &res) {
			auto &t = res.response.transfer;

			ASSERT_EQ(t.rxBufferSize,   12);

			ASSERT_TRUE(t.rxBuffer != NULL);

			ASSERT_EQ(t.rxBuffer[0], 0x10);
			ASSERT_EQ(t.rxBuffer[1], 0x20);
		}
	),

	ResponseTestParameters(
		PROTO_CMD_GET_INFO, PROTO_ERROR_INVALID_CRC, 0x45,

		{},
		{},
		[](ProtoRes &res) {
			ASSERT_EQ(res.code, PROTO_ERROR_INVALID_CRC);
		}
	)
));
