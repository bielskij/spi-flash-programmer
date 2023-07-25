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
		ProtoPkt pkt;

		proto_pkt_init(&pkt, txBuffer, sizeof(txBuffer), 0);

		ASSERT_EQ(pkt.payload,     nullptr);
		ASSERT_EQ(pkt.payloadSize, 0);

		pkt.code = PROTO_CMD_GET_INFO;
		pkt.id   = 0x12;

		txBufferWritten = proto_pkt_encode(&pkt);

		ASSERT_GT(txBufferWritten, 0);
	}

	{
		uint8_t rxBuffer[MEM_SIZE] = { 0 };

		ProtoPktDes ctx;
		ProtoPkt pkt;

		proto_pkt_dec_setup(&ctx, rxBuffer, sizeof(rxBuffer));

		pkt.payload     = (uint8_t *) 0x1234;
		pkt.payloadSize = 1231;

		for (size_t i = 0; i < sizeof(txBuffer); i++) {
			auto ret = proto_pkt_dec_putByte(&ctx, txBuffer[i], &pkt);

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
		ProtoPkt pkt;

		std::string payload = STRING_PAYLOAD_SHORT;

		ASSERT_TRUE(proto_pkt_init(&pkt, txBuffer, sizeof(txBuffer), payload.size()));

		ASSERT_NE(pkt.payload,     nullptr);
		ASSERT_NE(pkt.payloadSize, 0);

		pkt.code = PROTO_CMD_GET_INFO;
		pkt.id   = 0x12;

		memcpy(pkt.payload, payload.data(), payload.length());

		txBufferWritten = proto_pkt_encode(&pkt);

		ASSERT_GT(txBufferWritten, 0);
	}

	{
		uint8_t rxBuffer[MEM_SIZE] = { 0 };

		ProtoPktDes ctx;
		ProtoPkt pkt;

		proto_pkt_dec_setup(&ctx, rxBuffer, sizeof(rxBuffer));

		pkt.payload     = (uint8_t *) 0x1234;
		pkt.payloadSize = 1231;

		for (size_t i = 0; i < sizeof(txBuffer); i++) {
			auto ret = proto_pkt_dec_putByte(&ctx, txBuffer[i], &pkt);

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
		ProtoPkt pkt;

		std::string payload = STRING_PAYLOAD_LONG;

		ASSERT_TRUE(proto_pkt_init(&pkt, txBuffer, sizeof(txBuffer), payload.size()));

		ASSERT_NE(pkt.payload,     nullptr);
		ASSERT_NE(pkt.payloadSize, 0);

		pkt.code = PROTO_CMD_GET_INFO;
		pkt.id   = 0x12;

		memcpy(pkt.payload, payload.data(), payload.length());

		txBufferWritten = proto_pkt_encode(&pkt);

		ASSERT_GT(txBufferWritten, 0);
	}

	{
		uint8_t rxBuffer[MEM_SIZE] = { 0 };

		ProtoPktDes ctx;
		ProtoPkt pkt;

		proto_pkt_dec_setup(&ctx, rxBuffer, sizeof(rxBuffer));

		pkt.payload     = (uint8_t *) 0x1234;
		pkt.payloadSize = 1231;

		for (size_t i = 0; i < sizeof(txBuffer); i++) {
			auto ret = proto_pkt_dec_putByte(&ctx, txBuffer[i], &pkt);

			if (ret != PROTO_PKT_DES_RET_IDLE) {
				ASSERT_EQ(PROTO_PKT_DES_RET_GET_ERROR_CODE(ret), PROTO_NO_ERROR);

				ASSERT_EQ(pkt.code, PROTO_CMD_GET_INFO);
				ASSERT_EQ(pkt.id,   0x12);

				ASSERT_EQ(std::string((char *)pkt.payload, pkt.payloadSize), STRING_PAYLOAD_LONG);
			}
		}
	}
}


TEST(common_protocol, packet_error_payload_too_long) {
	uint8_t txBuffer[MEM_SIZE] = { 0 };
	size_t  txBufferWritten;

	// Request bigger than memory buffer
	{
		ProtoPkt pkt;

		pkt.code = PROTO_CMD_GET_INFO;
		pkt.id   = 0x12;

		ASSERT_FALSE(proto_pkt_init(&pkt, txBuffer, sizeof(txBuffer), sizeof(txBuffer) - 1));

		ASSERT_TRUE (proto_pkt_init(&pkt, txBuffer, sizeof(txBuffer), sizeof(txBuffer) - PROTO_FRAME_MIN_SIZE));

		txBufferWritten = proto_pkt_encode(&pkt);

		ASSERT_GT(txBufferWritten, 0);
	}

	{
		uint8_t rxBuffer[MEM_SIZE - PROTO_FRAME_MIN_SIZE - 1] = { 0 };

		ProtoPktDes ctx;
		ProtoPkt pkt;

		proto_pkt_dec_setup(&ctx, rxBuffer, sizeof(rxBuffer));

		pkt.payload     = (uint8_t *) 0x1234;
		pkt.payloadSize = 1231;

		for (size_t i = 0; i < sizeof(txBuffer); i++) {
			auto ret = proto_pkt_dec_putByte(&ctx, txBuffer[i], &pkt);

			if (ret != PROTO_PKT_DES_RET_IDLE) {
				ASSERT_EQ(PROTO_PKT_DES_RET_GET_ERROR_CODE(ret), PROTO_ERROR_INVALID_LENGTH);

				ASSERT_EQ(pkt.code, PROTO_CMD_GET_INFO);
				ASSERT_EQ(pkt.id,   0x12);
			}
		}
	}
}


TEST(common_protocol, packet_error_invalid_crc) {
	uint8_t txBuffer[MEM_SIZE] = { 0 };
	size_t  txBufferWritten;

	{
		ProtoPkt pkt;

		proto_pkt_init(&pkt, txBuffer, sizeof(txBuffer), 0);

		ASSERT_EQ(pkt.payload,     nullptr);
		ASSERT_EQ(pkt.payloadSize, 0);

		pkt.code = PROTO_CMD_GET_INFO;
		pkt.id   = 0x12;

		txBufferWritten = proto_pkt_encode(&pkt);

		ASSERT_GT(txBufferWritten, 0);

		txBuffer[txBufferWritten - 1] = 0xff;
	}

	{
		uint8_t rxBuffer[MEM_SIZE - PROTO_FRAME_MIN_SIZE - 1] = { 0 };

		ProtoPktDes ctx;
		ProtoPkt pkt;

		proto_pkt_dec_setup(&ctx, rxBuffer, sizeof(rxBuffer));

		pkt.payload     = (uint8_t *) 0x1234;
		pkt.payloadSize = 1231;

		for (size_t i = 0; i < sizeof(txBuffer); i++) {
			auto ret = proto_pkt_dec_putByte(&ctx, txBuffer[i], &pkt);

			if (ret != PROTO_PKT_DES_RET_IDLE) {
				ASSERT_EQ(PROTO_PKT_DES_RET_GET_ERROR_CODE(ret), PROTO_ERROR_INVALID_CRC);

				ASSERT_EQ(pkt.code, PROTO_CMD_GET_INFO);
				ASSERT_EQ(pkt.id,   0x12);
			}
		}
	}
}
