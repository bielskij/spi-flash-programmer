/*
 * id.h
 *
 *  Created on: 6 gru 2022
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef FLASH_ID_H_
#define FLASH_ID_H_

#include <cstdint>
#include <array>

namespace flashutil {
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
}

#endif /* FLASHUTIL_INCLUDE_FLASH_ID_H_ */
