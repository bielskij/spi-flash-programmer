/*
 * commands.cpp
 *
 *  Created on: 5 gru 2022
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#include "programmer/spi/commands.h"


struct flashutil::SpiCommands::Impl {
	SpiInterface &spi;

	Impl(SpiInterface &spi) : spi(spi) {
	}
};


flashutil::SpiCommands::SpiCommands(SpiInterface &interface) {
	this->self = std::make_unique<Impl>(interface);
}


flashutil::SpiCommands::~SpiCommands() {

}


bool flashutil::SpiCommands::getJedecId(uint8_t id[3]) {
	Buffer tx = { 0x9f };
	Buffer rx(3);

	if (this->self->spi.transfer(tx, &rx)) {
		id[0] = rx[0];
		id[1] = rx[1];
		id[2] = rx[2];

		return true;
	}

	return false;
}
