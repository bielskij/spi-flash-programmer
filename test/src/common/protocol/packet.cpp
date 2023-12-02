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
	std::vector<uint8_t> txBuffer(MEM_SIZE, 0);
	size_t               txBufferWritten;

	{
		ProtoPkt pkt;

		proto_pkt_init(&pkt, txBuffer.data(), txBuffer.size(), PROTO_CMD_GET_INFO, 0x12);

		ASSERT_EQ(pkt.code,        PROTO_CMD_GET_INFO);
		ASSERT_EQ(pkt.id,          0x12);
		ASSERT_EQ(pkt.payload,     nullptr);
		ASSERT_NE(pkt.payloadSize, 0);

		ASSERT_TRUE(proto_pkt_prepare(&pkt, txBuffer.data(), txBuffer.size(), 0));

		ASSERT_EQ(pkt.payload,     nullptr);
		ASSERT_EQ(pkt.payloadSize, 0);

		txBufferWritten = proto_pkt_encode(&pkt, txBuffer.data(), txBuffer.size());

		ASSERT_GT(txBufferWritten, 0);
	}

	{
		uint8_t rxBuffer[MEM_SIZE] = { 0 };

		ProtoPktDes ctx;
		ProtoPkt pkt;

		proto_pkt_dec_setup(&ctx, rxBuffer, sizeof(rxBuffer));

		pkt.payload     = (uint8_t *) 0x1234;
		pkt.payloadSize = 1231;

		size_t i;
		for (i = 0; i < txBufferWritten; i++) {
			auto ret = proto_pkt_dec_putByte(&ctx, txBuffer[i], &pkt);

			if (ret != PROTO_PKT_DES_RET_IDLE) {
				ASSERT_EQ(PROTO_PKT_DES_RET_GET_ERROR_CODE(ret), PROTO_NO_ERROR);

				ASSERT_EQ(pkt.code, PROTO_CMD_GET_INFO);
				ASSERT_EQ(pkt.id,   0x12);

				ASSERT_EQ(pkt.payload,     nullptr);
				ASSERT_EQ(pkt.payloadSize, 0);
				break;
			}
		}

		ASSERT_NE(i, txBufferWritten);
	}
}


TEST(common_protocol, packet_short_payload) {
	std::vector<uint8_t> txBuffer(MEM_SIZE, 0);
	size_t               txBufferWritten;

	{
		ProtoPkt pkt;

		std::string payload = STRING_PAYLOAD_SHORT;

		proto_pkt_init(&pkt, txBuffer.data(), txBuffer.size(), PROTO_CMD_GET_INFO, 0x12);

		ASSERT_EQ(pkt.payload,     nullptr);
		ASSERT_NE(pkt.payloadSize, 0);

		ASSERT_TRUE(proto_pkt_prepare(&pkt, txBuffer.data(), txBuffer.size(), payload.length()));

		ASSERT_NE(pkt.payload,     nullptr);
		ASSERT_NE(pkt.payloadSize, 0);

		memcpy(pkt.payload, payload.data(), payload.length());

		txBufferWritten = proto_pkt_encode(&pkt, txBuffer.data(), txBuffer.size());

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
	std::vector<uint8_t> txBuffer(MEM_SIZE, 0);
	size_t               txBufferWritten;

	{
		ProtoPkt pkt;

		std::string payload = STRING_PAYLOAD_LONG;

		proto_pkt_init(&pkt, txBuffer.data(), txBuffer.size(), PROTO_CMD_GET_INFO, 0x12);

		ASSERT_EQ(pkt.payload,     nullptr);
		ASSERT_NE(pkt.payloadSize, 0);

		ASSERT_TRUE(proto_pkt_prepare(&pkt, txBuffer.data(), txBuffer.size(), payload.length()));

		ASSERT_NE(pkt.payload,     nullptr);
		ASSERT_NE(pkt.payloadSize, 0);

		memcpy(pkt.payload, payload.data(), payload.length());

		txBufferWritten = proto_pkt_encode(&pkt, txBuffer.data(), txBuffer.size());

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
	std::vector<uint8_t> txBuffer(MEM_SIZE, 0);
	size_t               txBufferWritten;

	// Request bigger than memory buffer
	{
		ProtoPkt pkt;

		proto_pkt_init(&pkt, txBuffer.data(), txBuffer.size(), PROTO_CMD_GET_INFO, 0x12);

		ASSERT_FALSE(proto_pkt_prepare(&pkt, txBuffer.data(), txBuffer.size(), txBuffer.size() - 1));
		ASSERT_TRUE (proto_pkt_prepare(&pkt, txBuffer.data(), txBuffer.size(), txBuffer.size() - PROTO_FRAME_MIN_SIZE));

		txBufferWritten = proto_pkt_encode(&pkt, txBuffer.data(), txBuffer.size());

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
	std::vector<uint8_t> txBuffer(MEM_SIZE, 0);
	size_t               txBufferWritten;

	{
		ProtoPkt pkt;

		proto_pkt_init(&pkt, txBuffer.data(), txBuffer.size(), PROTO_CMD_GET_INFO, 0x12);

		ASSERT_EQ(pkt.payload,     nullptr);
		ASSERT_NE(pkt.payloadSize, 0);

		ASSERT_TRUE(proto_pkt_prepare(&pkt, txBuffer.data(), txBuffer.size(), 0));

		ASSERT_EQ(pkt.payload,     nullptr);
		ASSERT_EQ(pkt.payloadSize, 0);

		txBufferWritten = proto_pkt_encode(&pkt, txBuffer.data(), txBuffer.size());

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
