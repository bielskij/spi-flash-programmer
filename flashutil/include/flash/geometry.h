/*
 * geometry.h
 *
 *  Created on: 5 gru 2022
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef FLASH_GEOMETRY_H_
#define FLASH_GEOMETRY_H_

#include <cstdlib>

namespace flashutil {
	class FlashGeometry {
		public:
			FlashGeometry() {
				this->blockSize   = 0;
				this->blockCount  = 0;
				this->sectorSize  = 0;
				this->sectorCount = 0;
				this->pageSize    = 0;
			}

		public:
			size_t getBlockSize() const {
				return this->blockSize;
			}

			size_t getBlockCount() const {
				return this->blockCount;
			}

			size_t getSectorSize() const {
				return this->sectorSize;
			}

			size_t getSectorCount() const {
				return this->sectorCount;
			}

			size_t getPageSize() const {
				return this->pageSize;
			}

		private:
			size_t blockSize;
			size_t blockCount;
			size_t sectorSize;
			size_t sectorCount;
			size_t pageSize;
	};
}

#endif /* FLASH_GEOMETRY_H_ */
