/*
 * serial.h
 *
 *  Created on: 5 gru 2022
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef PROGRAMMER_SPI_SERIAL_H_
#define PROGRAMMER_SPI_SERIAL_H_

#include <string>
#include <memory>

#include "programmer/spi/interface.h"


namespace flashutil {
	class SerialSpi : public SpiInterface {
		public:
			SerialSpi(const std::string &serial);
			~SerialSpi();

		public:
			virtual bool transfer(const Buffer &tx, Buffer *rx) override;

		private:
			struct Impl;

			std::unique_ptr<Impl> self;
	};
}

#endif /* PROGRAMMER_SPI_SERIAL_H_ */
