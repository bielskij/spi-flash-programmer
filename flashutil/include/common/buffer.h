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
			Buffer(size_t size) {
				this->_v.reserve(size);
			}

			void fill(uint8_t value) {
				std::fill_n(this->_v.begin(), this->_v.size(), value);
			}

		private:
			std::vector<uint8_t> _v;
	};
}

#endif /* COMMON_BUFFER_H_ */
