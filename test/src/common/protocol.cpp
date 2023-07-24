#include <gtest/gtest.h>

#include "common/protocol.h"
#include "common/protocol/packet.h"


#define STRING_PAYLOAD_SHORT "this is a sample payload string"
#define STRING_PAYLOAD_LONG \
	"long payload, long payload, long payload, long payload, long payload, long payload, long payload, long payload" \
	"long payload, long payload, long payload, long payload, long payload, long payload, long payload, long payload" \
	"long payload, long payload, long payload, long payload, long payload, long payload, long payload, long payload" \
	"long payload, long payload, long payload, long payload, long payload, long payload, long payload, long payload"


#define MEM_SIZE 512


TEST(common_protocol, packet_no_payload) {
	uint8_t txBuffer[MEM_SIZE] = { 0 };
	size_t  txBufferWritten;

	{
		ProtoPkt req;

		req.code        = PROTO_CMD_GET_INFO;
		req.id          = 0x12;
		req.payload     = NULL;
		req.payloadSize = 0;

		txBufferWritten = proto_pkt_ser(&req, txBuffer, sizeof(txBuffer));

		ASSERT_GT(txBufferWritten, 0);
	}

	{
		uint8_t rxBuffer[MEM_SIZE] = { 0 };

		ProtoPktDes ctx;
		ProtoPkt pkt;

		proto_pkt_des_setup(&ctx, rxBuffer, sizeof(rxBuffer));

		pkt.payload     = (uint8_t *) 0x1234;
		pkt.payloadSize = 1231;

		for (size_t i = 0; i < sizeof(txBuffer); i++) {
			auto ret = proto_pkt_des_putByte(&ctx, txBuffer[i], &pkt);

			if (ret != PROTO_PKT_DES_RET_IDLE) {
				ASSERT_EQ(PROTO_PKT_DES_RET_GET_ERROR_CODE(ret), PROTO_NO_ERROR);

				ASSERT_EQ(pkt.code, PROTO_CMD_GET_INFO);
				ASSERT_EQ(pkt.id,   0x12);

				ASSERT_EQ(pkt.payload,     nullptr);
				ASSERT_EQ(pkt.payloadSize, 0);
			}
		}
	}
}


TEST(common_protocol, packet_short_payload) {
	uint8_t txBuffer[MEM_SIZE] = { 0 };
	size_t  txBufferWritten;

	{
		ProtoPkt req;

		std::string payload = STRING_PAYLOAD_SHORT;

		req.code        = PROTO_CMD_GET_INFO;
		req.id          = 0x12;
		req.payload     = payload.data();
		req.payloadSize = payload.length();

		txBufferWritten = proto_pkt_ser(&req, txBuffer, sizeof(txBuffer));

		ASSERT_GT(txBufferWritten, 0);
	}

	{
		uint8_t rxBuffer[MEM_SIZE] = { 0 };

		ProtoPktDes ctx;
		ProtoPkt pkt;

		proto_pkt_des_setup(&ctx, rxBuffer, sizeof(rxBuffer));

		pkt.payload     = (uint8_t *) 0x1234;
		pkt.payloadSize = 1231;

		for (size_t i = 0; i < sizeof(txBuffer); i++) {
			auto ret = proto_pkt_des_putByte(&ctx, txBuffer[i], &pkt);

			if (ret != PROTO_PKT_DES_RET_IDLE) {
				ASSERT_EQ(PROTO_PKT_DES_RET_GET_ERROR_CODE(ret), PROTO_NO_ERROR);

				ASSERT_EQ(pkt.code, PROTO_CMD_GET_INFO);
				ASSERT_EQ(pkt.id,   0x12);

				ASSERT_EQ(std::string((char *)pkt.payload, pkt.payloadSize), STRING_PAYLOAD_SHORT);
			}
		}
	}
}


TEST(common_protocol, packet_long_payload) {
	uint8_t txBuffer[MEM_SIZE] = { 0 };
	size_t  txBufferWritten;

	{
		ProtoPkt req;

		std::string payload = STRING_PAYLOAD_LONG;

		req.code        = PROTO_CMD_GET_INFO;
		req.id          = 0x12;
		req.payload     = payload.data();
		req.payloadSize = payload.length();

		txBufferWritten = proto_pkt_ser(&req, txBuffer, sizeof(txBuffer));

		ASSERT_GT(txBufferWritten, 0);
	}

	{
		uint8_t rxBuffer[MEM_SIZE] = { 0 };

		ProtoPktDes ctx;
		ProtoPkt pkt;

		proto_pkt_des_setup(&ctx, rxBuffer, sizeof(rxBuffer));

		pkt.payload     = (uint8_t *) 0x1234;
		pkt.payloadSize = 1231;

		for (size_t i = 0; i < sizeof(txBuffer); i++) {
			auto ret = proto_pkt_des_putByte(&ctx, txBuffer[i], &pkt);

			if (ret != PROTO_PKT_DES_RET_IDLE) {
				ASSERT_EQ(PROTO_PKT_DES_RET_GET_ERROR_CODE(ret), PROTO_NO_ERROR);

				ASSERT_EQ(pkt.code, PROTO_CMD_GET_INFO);
				ASSERT_EQ(pkt.id,   0x12);

				ASSERT_EQ(std::string((char *)pkt.payload, pkt.payloadSize), STRING_PAYLOAD_LONG);
			}
		}
	}
}
