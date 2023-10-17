/*
 * creator.h
 *
 *  Created on: 6 mar 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef SPI_CREATOR_H_
#define SPI_CREATOR_H_

#include <memory>

#include "flashutil/spi.h"


class SpiCreator {
	public:
		SpiCreator();
		virtual ~SpiCreator();

		std::unique_ptr<Spi> createSerialSpi(const std::string &path, int baudrate);
};


#endif /* SPI_CREATOR_H_ */
