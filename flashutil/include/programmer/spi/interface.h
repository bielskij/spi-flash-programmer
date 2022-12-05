/*
 * spi.h
 *
 *  Created on: 5 gru 2022
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef PROGRAMMER_SPI_INTERFACE_H_
#define PROGRAMMER_SPI_INTERFACE_H_

#include <memory>

#include "common/buffer.h"

namespace flashutil {
	class SpiInterface {
		public:
			virtual ~SpiInterface() {
			}

		public:
			virtual bool transfer(const Buffer &tx, Buffer *rx) = 0;

		public:
			static std::unique_ptr<SpiInterface> createSerial(const std::string &path);
	};
}

#endif /* PROGRAMMER_SPI_INTERFACE_H_ */
