/*
 * flash.h
 *
 *  Created on: 8 wrz 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */


#ifndef FLASH_H_
#define FLASH_H_

#include <string>
#include <array>

class Flash {
	public:
		Flash(const std::string &name, uint32_t jedecId, uint16_t extId, size_t blockSize, size_t nblocks, size_t sectorSize, size_t nSectors, uint8_t protectMask);

	private:
		std::string            name;
		std::array<uint8_t, 5> id;
		char                   idLen;
		size_t                 blockSize;
		size_t                 blockCount;
		size_t                 sectorSize;
		size_t                 sectorCount;
		size_t                 pageSize;
		uint8_t                protectMask;
};


#endif /* FLASH_H_ */
