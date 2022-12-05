/*
 * spi.h
 *
 *  Created on: 5 gru 2022
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef PROGRAMMER_SPI_H_
#define PROGRAMMER_SPI_H_

#include "common/buffer.h"

namespace flashutil {
	class SpiInterface {
		public:
			virtual ~SpiInterface() {
			}

		public:
			virtual bool transfer(const Buffer &tx, Buffer *rx) = 0;
	};
}

#endif /* PROGRAMMER_SPI_H_ */
