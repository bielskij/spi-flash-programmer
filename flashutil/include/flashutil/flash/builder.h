/*
 * flashutil/flash/registry.h
 *
 *  Created on: 8 wrz 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef FLASHUTIL_FLASH_BUILDER_H_
#define FLASHUTIL_FLASH_BUILDER_H_

#include <string>
#include <vector>
#include <cstddef>

#include "flashutil/flash.h"

class FlashBuilder {
	public:
		FlashBuilder();

		FlashBuilder &setName(const std::string &name);
		FlashBuilder &setJedecId(const std::vector<uint8_t> &jedec);
		FlashBuilder &setBlockSize(std::size_t size);
		FlashBuilder &setBlockCount(std::size_t count);
		FlashBuilder &setSectorSize(std::size_t size);
		FlashBuilder &setSectorCount(std::size_t count);
		FlashBuilder &setPageSize(std::size_t size);
		FlashBuilder &setPageCount(std::size_t count);
		FlashBuilder &setSize(std::size_t size);
		FlashBuilder &setProtectMask(uint8_t mask);
		FlashBuilder &reset();

		Flash build();

	private:
		Flash flash;
};

#endif /* FLASHUTIL_FLASH_BUILDER_H_ */
