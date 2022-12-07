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
			FlashGeometry(size_t bs, size_t bc, size_t ss, size_t sc, size_t ps = 256) {
				this->blockSize   = bs;
				this->blockCount  = bc;
				this->sectorSize  = ss;
				this->sectorCount = sc;
				this->pageSize    = ps;
			}

			FlashGeometry() : FlashGeometry(0, 0, 0, 0) {
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

			size_t getTotalSize() const {
				if (this->blockCount > 0 && this->blockSize > 0) {
					return this->blockCount * this->blockSize;
				}

				if (this->sectorCount > 0 && this->sectorSize > 0) {
					return this->sectorCount * this->sectorSize;
				}

				return 0;
			}

			bool isValid() const {
				size_t totalSizeFromBlocks  = this->blockCount  * this->blockSize;
				size_t totalSizeFromSectors = this->sectorCount * this->sectorSize;

				if (totalSizeFromBlocks > 0) {
					if (totalSizeFromSectors > 0) {
						return totalSizeFromBlocks == totalSizeFromSectors;

					} else {
						return true;
					}

				} else if (totalSizeFromSectors > 0) {
					return true;

				}

				return false;
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
