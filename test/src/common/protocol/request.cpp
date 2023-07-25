#include <gtest/gtest.h>

#include "common/protocol.h"
#include "common/protocol/packet.h"
#include "common/protocol/request.h"


TEST(common_protocol, request_getInfo) {
	uint8_t  txBuffer[1024];
	uint16_t txBufferWritten;
	uint8_t  rxBuffer[1024];

	ProtoPkt pkt;

	{
		ProtoReq req;

		proto_req_init(&req, PROTO_CMD_SPI_TRANSFER, 0x45);

		req.request.transfer.flags = 0;

		req.request.transfer.txBufferSize = 10;
		req.request.transfer.rxBufferSize = 12;
		req.request.transfer.rxSkipSize   = 2;

		ASSERT_TRUE(proto_pkt_init(&pkt, txBuffer, sizeof(txBuffer), proto_req_getPayloadSize(&req)));
		{
			proto_req_assign(&req, pkt.payload, pkt.payloadSize, false);
			{
				auto &t = req.request.transfer;

				t.txBuffer[0] = 0x10;
				t.txBuffer[1] = 0x20;
			}
			proto_req_encode(&req, pkt.payload, pkt.payloadSize);

			txBufferWritten = proto_pkt_encode(&pkt);
		}

		ASSERT_GT(txBufferWritten, 0);
	}

	{
		ProtoPktDes decoder;

		proto_pkt_dec_setup(&decoder, rxBuffer, sizeof(rxBuffer));

		for (uint16_t i = 0; i < txBufferWritten; i++) {
			auto ret = proto_pkt_dec_putByte(&decoder, txBuffer[i], &pkt);

			if (ret != PROTO_PKT_DES_RET_IDLE) {
				ProtoReq req;

				ASSERT_EQ(PROTO_PKT_DES_RET_GET_ERROR_CODE(ret), PROTO_NO_ERROR);

				ASSERT_EQ(pkt.code, PROTO_CMD_SPI_TRANSFER);
				ASSERT_EQ(pkt.id,   0x45);

				ASSERT_NE(pkt.payload,     nullptr);
				ASSERT_NE(pkt.payloadSize, 0);

				proto_req_init  (&req, pkt.code, pkt.id);
				proto_req_decode(&req, pkt.payload, pkt.payloadSize);
				proto_req_assign(&req, pkt.payload, pkt.payloadSize, true);

				ASSERT_EQ(req.cmd, PROTO_CMD_SPI_TRANSFER);
				ASSERT_EQ(req.id,  0x45);
			}
		}

	}
}
