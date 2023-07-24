#include <gtest/gtest.h>

#include "common/protocol.h"
#include "common/protocol/response.h"


TEST(common_protocol, response_getInfo) {
	uint8_t  buffer[128];
	ProtoPkt pkt;

	{
		ProtoRes res;

		res.cmd = PROTO_CMD_GET_INFO;
		res.id  = 0x87;

		res.response.getInfo.payloadSize   = 1024;
		res.response.getInfo.version.major = 1;
		res.response.getInfo.version.minor = 2;

		ASSERT_TRUE(proto_res_enc(&res, &pkt, buffer, sizeof(buffer)));
	}

	{
		ProtoRes dst;

		ASSERT_TRUE(proto_res_dec(&dst, &pkt));

		ASSERT_EQ(dst.cmd, PROTO_CMD_GET_INFO);
		ASSERT_EQ(dst.id,  0x87);

		ASSERT_EQ(dst.response.getInfo.payloadSize,   1024);
		ASSERT_EQ(dst.response.getInfo.version.major, 1);
		ASSERT_EQ(dst.response.getInfo.version.minor, 2);
	}
}
