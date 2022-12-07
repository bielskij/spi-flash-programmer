/*
 * buffer.h
 *
 *  Created on: 5 gru 2022
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef COMMON_BUFFER_H_
#define COMMON_BUFFER_H_

#include <vector>
#include <cstdint>
#include <cstdlib>

namespace flashutil {
	class Buffer {
		public:
			Buffer() : Buffer(0) {
			}

			Buffer(size_t size) : _v(size) {
			}

			void fill(uint8_t value) {
				std::fill_n(this->_v.begin(), this->_v.size(), value);
			}

			void reset() {
				this->_v.clear();
			}

			void resize(size_t size) {
				this->_v.reserve(size);
			}

			uint8_t &at(size_t index) {
				return this->_v.at(index);
			}

			const uint8_t &at(size_t index) const {
				return this->_v.at(index);
			}

			uint8_t &operator[](size_t index) {
				return _v[index];
			}

			const uint8_t &operator[](size_t index) const {
				return _v[index];
			}

			size_t size() const {
				return _v.size();
			}

			uint8_t *data() {
				return _v.data();
			}

			const uint8_t *data() const {
				return _v.data();
			}

		private:
			std::vector<uint8_t> _v;
	};
}

#endif /* COMMON_BUFFER_H_ */
