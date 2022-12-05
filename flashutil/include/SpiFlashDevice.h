/*
 * SpiFlashDevice.h
 *
 *  Created on: 5.12.2022
 *      Author: bielski.j@gmail.com
 */

#ifndef SPIFLASHDEVICE_H_
#define SPIFLASHDEVICE_H_

#include <string>

class SpiFlashDevice {
	public:
		class Id {
			public:
				Id() : Id(nullptr, nullptr) {
				}

				Id(const uint8_t jedecId[3], const uint8_t extId[2]) {
					this->setValues(jedecId, extId);
				}

				Id(uint32_t jedecId, uint16_t extId) {
					uint8_t jId[3] = {
						uint8_t((jedecId >> 16) & 0xff),
						uint8_t((jedecId >>  8) & 0xff),
						uint8_t((jedecId >>  0) & 0xff)
					};

					uint8_t eId[2] = {
						uint8_t((extId >> 8) & 0xff),
						uint8_t((extId >> 0) & 0xff)
					};

					this->setValues(jId, eId);
				}

				uint32_t getJedecId() const {
					return (this->id[0] << 16) | (this->id[1] << 8) | this->id[2];
				}

				uint16_t getExtendedId() const {
					return (this->id[3] << 8) | this->id[4];
				}

				uint8_t getManufacturerId() const {
					return this->id[0];
				}

				uint8_t getMemoryType() const {
					return this->id[1];
				}

				uint8_t getCapacity() const {
					return this->id[2];
				}

				bool isNull() const {
					return this->id[0] == 0 || this->id[0] == 0xff;
				}

				bool operator==(const Id &other) const {
					return
						(this->idLen == other.idLen) &&
						std::equal(this->id.begin(), this->id.end(), other.id.begin());
				}

			private:
				void setValues(const uint8_t jedecId[3], const uint8_t extId[2]) {
					if (jedecId == nullptr) {
						this->idLen = 0;

					} else {
						if (jedecId[0] == 0 || jedecId[1] == 0 || jedecId[2]) {
							this->idLen = 0;

						} else {
							this->idLen = 3;

							this->id[0] = jedecId[0];
							this->id[1] = jedecId[1];
							this->id[2] = jedecId[2];
						}

						if (extId != nullptr) {
							if (extId[0] != 0 || extId[1] != 0) {
								this->idLen += 2;

								this->id[3] = extId[0];
								this->id[4] = extId[1];
							}
						}
					}
				}

			private:
				std::array<uint8_t, 5> id;;
				char                   idLen;
		};

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

		const Id &getId() const {
			return this->id;
		}

		void setId(const Id &id) {
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
		Id id;

		std::string name;

		size_t  blockSize;
		size_t  blockCount;
		size_t  sectorSize;
		size_t  sectorCount;
		size_t  pageSize;
		uint8_t protectMask;
};


#endif /* SPIFLASHDEVICE_H_ */
