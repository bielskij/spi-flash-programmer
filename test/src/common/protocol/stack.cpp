#include <gtest/gtest.h>

#include "common/protocol.h"
#include "common/protocol/request.h"
#include "common/protocol/response.h"


TEST(common_protocol, stack_getInfo) {
	uint8_t txBuffer[1024];
	uint8_t rxBuffer[1024];

	{
		ProtoPkt pkt;

		{
			ProtoReq req;

			req.cmd = PROTO_CMD_GET_INFO;
			req.id  = 0x45;

			ASSERT_TRUE(proto_req_enc(&req, &pkt, txBuffer, sizeof(txBuffer)));
		}

		proto_pkt_enc(&pkt, txBuffer, sizeof(txBuffer));
	}
}
