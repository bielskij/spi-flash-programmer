/*
 * SpiFlashDevice.h
 *
 *  Created on: 5.12.2022
 *      Author: bielski.j@gmail.com
 */

#ifndef SPIFLASHDEVICE_H_
#define SPIFLASHDEVICE_H_

#include <string>
#include "flash/id.h"


class SpiFlashDevice {
	public:

	public:
		SpiFlashDevice(const std::string &name) : name(name) {
			this->blockSize   = 0;
			this->blockCount  = 0;
			this->sectorSize  = 0;
			this->sectorCount = 0;
			this->protectMask = 0;
			this->pageSize    = 256;
		}

		SpiFlashDevice(const std::string &name, uint32_t jedecId, uint16_t extId, size_t blockSize, size_t blockCount, size_t sectorSize, size_t sectorCount, uint8_t protectMask) : id(jedecId, extId), name(name) {
			this->blockSize   = blockSize;
			this->blockCount  = blockCount;
			this->sectorSize  = sectorSize;
			this->sectorCount = sectorCount;
			this->protectMask = protectMask;
			this->pageSize    = 256;
		};

		const flashutil::Id &getId() const {
			return this->id;
		}

		void setId(const flashutil::Id &id) {
			this->id = id;
		}

		const std::string &getName() const {
			return this->name;
		}

		const size_t getBlockSize() const {
			return this->blockSize;
		}

		const size_t getBlockCount() const {
			return this->blockCount;
		}

		const size_t getSectorSize() const {
			return this->sectorSize;
		}

		const size_t getSectorCount() const {
			return this->sectorCount;
		}

		const size_t getPageSize() const {
			return this->pageSize;
		}

		const uint8_t getProtectMask() const {
			return this->protectMask;
		}

	private:
		flashutil::Id id;

		std::string name;

		size_t  blockSize;
		size_t  blockCount;
		size_t  sectorSize;
		size_t  sectorCount;
		size_t  pageSize;
		uint8_t protectMask;
};


#endif /* SPIFLASHDEVICE_H_ */
