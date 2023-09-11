/*
 * flash/registry.h
 *
 *  Created on: 8 wrz 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef FLASHUTIL_INCLUDE_FLASH_BUILDER_H_
#define FLASHUTIL_INCLUDE_FLASH_BUILDER_H_

#include <string>
#include <vector>

#include "flash.h"

class FlashBuilder {
	public:
		FlashBuilder();

		FlashBuilder &setName(const std::string &name);
		FlashBuilder &setJedecId(const std::vector<uint8_t> &jedec);
		FlashBuilder &setBlockSize(size_t size);
		FlashBuilder &setBlockCount(size_t count);
		FlashBuilder &setSectorSize(size_t size);
		FlashBuilder &setSectorCount(size_t count);
		FlashBuilder &setSize(size_t size);
		FlashBuilder &setProtectMask(uint8_t mask);
		FlashBuilder &reset();

		Flash build();

	private:
		Flash flash;
};

#endif /* FLASHUTIL_INCLUDE_FLASH_BUILDER_H_ */
