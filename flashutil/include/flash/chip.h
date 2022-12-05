/*
 * chip.h
 *
 *  Created on: 5 gru 2022
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef FLASH_CHIP_H_
#define FLASH_CHIP_H_

#include "flash/geometry.h"

namespace flashutil {
	class FlashChip {
		public:
			FlashChip(const FlashGeometry &geometry);

		public:
			const FlashGeometry &getFlashGeometry() const;
			FlashGeometry &getFlashGeometry();
	};
}

#endif /* FLASH_CHIP_H_ */
