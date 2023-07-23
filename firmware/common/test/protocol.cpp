/*
 * protocol.cpp
 *
 *  Created on: 12 cze 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#include <gtest/gtest.h>

#include "protocol.h"
#include "crc8.h"
#include "firmware/programmer.h"


static void _spiTransferCallback(void *buffer, uint16_t txSize, uint16_t rxSize, void *callbackData) {
	std::cout << __func__ << std::endl;
}


static void _spiCsCallback(bool assert, void *callbackData) {
	std::cout << __func__ << ", assert: " << std::to_string(assert) << std::endl;
}


static void _serialSendCallback(uint8_t data, void *callbackData) {
	std::cout << __func__ << " " << std::hex << (int) data << std::dec << std::endl;
}


static void _serialFlushCallback(void *callbackData) {
	std::cout << __func__ << std::endl;
}


TEST(firmware_common, protocol) {
	uint8_t memory[1024];

	{
		ProtocolReveicerSetupParameters params;

		params.callbackData        = nullptr;
		params.memory              = memory;
		params.memorySize          = sizeof(memory);
		params.spiCsCallback       = _spiCsCallback;
		params.spiTransferCallback = _spiTransferCallback;
		params.serialFlushCallback = _serialFlushCallback;
		params.serialSendCallback  = _serialSendCallback;

		ASSERT_TRUE(programmer_setup(&params));
	}

	{
		std::vector<uint8_t> data = {
			PROTO_SYNC_NIBBLE | PROTO_CMD_GET_INFO, 0xa5, 0x00, 0x00
		};

		*data.rbegin() = crc8_get(data.data(), data.size() - 1, PROTO_CRC8_POLY, PROTO_CRC8_START);

		for (auto b : data) {
			programmer_onByte(b);
		}
	}
}
