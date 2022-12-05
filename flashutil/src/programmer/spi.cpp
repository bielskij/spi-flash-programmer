/*
 * spi.cpp
 *
 *  Created on: 5 gru 2022
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#include "programmer/spi/interface.h"
#include "spi/impl/serial.h"


std::unique_ptr<flashutil::SpiInterface> flashutil::SpiInterface::createSerial(const std::string &path) {
	return std::make_unique<flashutil::SerialSpi>(path);
}
