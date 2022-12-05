/*
 * Spi.h
 *
 *  Created on: 5 gru 2022
 *      Author: bielski.j@gmail.com
 */

#ifndef SPI_H_
#define SPI_H_

#include <memory>

namespace flashutil {
	class SpiInterface {
		public:
			virtual ~SpiInterface() {}

			static std::unique_ptr<SpiInterface> createSerial();

		public:
			virtual bool transfer() = 0;
	};
}


#endif /* SPI_H_ */
