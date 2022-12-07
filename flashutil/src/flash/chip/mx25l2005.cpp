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

			BasicOperations::getOpcodes(),
			std::make_unique<BasicOperations>()
		)
	);
}
