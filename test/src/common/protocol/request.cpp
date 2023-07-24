#include <gtest/gtest.h>

#include "common/protocol.h"
#include "common/protocol/request.h"


TEST(common_protocol, request_getInfo) {
	uint8_t  buffer[128];
	ProtoPkt pkt;

	{
		ProtoReq src;

		src.cmd = PROTO_CMD_GET_INFO;
		src.id  = 0x87;

		ASSERT_TRUE(proto_req_enc(&src, &pkt, buffer, sizeof(buffer)));
	}

	{
		ProtoReq dst;

		ASSERT_TRUE(proto_req_dec(&dst, &pkt));

		ASSERT_EQ(dst.cmd, PROTO_CMD_GET_INFO);
		ASSERT_EQ(dst.id,  0x87);
	}
}
