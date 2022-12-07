/*
 * mx25l20xx.cpp
 *
 *  Created on: 6 gru 2022
 *      Author: jarko
 */

#include "common/utils.h"

#include "flash/library.h"
#include "flash/operations/basic.h"

using namespace flashutil;

FLASHUTIL_SPEC_REGISTER() {
	FlashLibrary::getInstance().addFlashSpec(
		FlashSpec(
			"Macronix MX25L2026E/MX25l2005A",

			Id(0xc22012, 0),
			FlashGeometry(KB(64), 4, KB(4), 64),
			FlashProtection(0x8c),

			{
				FlashOpcode::H_05_READ_STATUS_REG,
				FlashOpcode::H_01_WRITE_STATUS_REG,
				FlashOpcode::H_9F_GET_JEDEC_ID
			},
			nullptr
		)
	);
}
