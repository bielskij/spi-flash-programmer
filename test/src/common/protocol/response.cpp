#include <gtest/gtest.h>

#include "common/protocol.h"
#include "common/protocol/response.h"
#include "common/protocol/packet.h"


TEST(common_protocol, response_getInfo) {
	uint8_t  txBuffer[1024];
	uint16_t txBufferWritten;
	uint8_t  rxBuffer[1024];

	ProtoPkt pkt;

	{
		ProtoRes res;

		proto_res_init(&res, PROTO_CMD_GET_INFO, 0x87);

		res.response.getInfo.payloadSize   = 1024;
		res.response.getInfo.version.major = 1;
		res.response.getInfo.version.minor = 2;

		ASSERT_TRUE(proto_pkt_init(&pkt, txBuffer, sizeof(txBuffer), res.cmd, res.id, proto_res_getPayloadSize(&res)));
		{
			proto_res_assign(&res, pkt.payload, pkt.payloadSize, false);
			{

			}
			ASSERT_EQ(proto_res_encode(&res, pkt.payload, pkt.payloadSize), pkt.payloadSize);

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
				ProtoRes res;

				ASSERT_EQ(PROTO_PKT_DES_RET_GET_ERROR_CODE(ret), PROTO_NO_ERROR);

				ASSERT_EQ(pkt.code, PROTO_CMD_GET_INFO);
				ASSERT_EQ(pkt.id,   0x87);

				ASSERT_NE(pkt.payload,     nullptr);
				ASSERT_NE(pkt.payloadSize, 0);

				proto_res_init  (&res, pkt.code, pkt.id);
				proto_res_decode(&res, pkt.payload, pkt.payloadSize);
				proto_res_assign(&res, pkt.payload, pkt.payloadSize, true);

				ASSERT_EQ(res.cmd, PROTO_CMD_GET_INFO);
				ASSERT_EQ(res.id,  0x87);

				ASSERT_EQ(res.response.getInfo.payloadSize, 1024);
				ASSERT_EQ(res.response.getInfo.version.major, 1);
				ASSERT_EQ(res.response.getInfo.version.minor, 2);
				break;
			}
		}

		ASSERT_NE(i, txBufferWritten);
	}
}
