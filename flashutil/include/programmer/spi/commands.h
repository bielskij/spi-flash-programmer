/*
 * commands.h
 *
 *  Created on: 5 gru 2022
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef PROGRAMMER_SPI_COMMANDS_H_
#define PROGRAMMER_SPI_COMMANDS_H_

#include "programmer/spi/interface.h"


namespace flashutil {
	class SpiCommands {
		public:
			SpiCommands(SpiInterface &interface);

		public:
			bool getJedecId(uint8_t id[3]);

		private:
			class Impl;

			std::unique_ptr<Impl> self;
	};
}

#endif /* PROGRAMMER_SPI_COMMANDS_H_ */
