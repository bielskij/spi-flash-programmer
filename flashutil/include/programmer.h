/*
 * programmer.h
 *
 *  Created on: 8 wrz 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef PROGRAMMER_H_
#define PROGRAMMER_H_

#include "spi.h"

class Programmer {
	public:
		Programmer(Spi *spiDev);

		void begin();
		void end();

		void eraseChip();

		void eraseBlockByAddress(uint32_t address);
		void eraseBlockByNumber(int blockNo);

		void eraseSectorByAddress(uint32_t addres);
		void eraseSectorByNumber(int sectorNo);

	private:
		void cmdEraseBlock(uint32_t address);
		void cmdEraseSector(uint32_t address);


	private:
		int _maxReconnections;
};


#endif /* PROGRAMMER_H_ */
