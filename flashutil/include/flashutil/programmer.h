/*
 * programmer.h
 *
 *  Created on: 8 wrz 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef PROGRAMMER_H_
#define PROGRAMMER_H_

#include "spi.h"
#include "flash/registry.h"


class Programmer {
	public:
		Programmer(Spi &spiDev, const FlashRegistry *registry);
		virtual ~Programmer();

		void begin(const Flash *defaultGeometry);
		void end();

		void unlockChip();

		void eraseChip();

		void eraseBlockByAddress(uint32_t address, bool skipIfErased);
		void eraseBlockByNumber(int blockNo, bool skipIfErased);

		void eraseSectorByAddress(uint32_t addres, bool skipIfErased);
		void eraseSectorByNumber(int sectorNo, bool skipIfErased);

		const Flash &getFlashInfo() const;

	private:
		void verifyFlashInfoAreaByAddress(uint32_t address, size_t size, size_t alignment);
		void verifyFlashInfoBlockNo(int blockNo);
		void verifyFlashInfoSectorNo(int sectorNo);

		bool checkErased(uint32_t address, size_t size);
		void waitForWIPClearance(int timeoutMs);

		void cmdEraseChip();
		void cmdEraseBlock(uint32_t address);
		void cmdEraseSector(uint32_t address);
		void cmdGetInfo(std::vector<uint8_t> &id);
		void cmdGetStatus(uint8_t &reg);
		void cmdWriteEnable();
		void cmdWriteStatus(uint8_t reg);
		void cmdFlashReadBegin(uint32_t address);

	private:
		Flash                _flashInfo;
		const FlashRegistry *_flashRegistry;

		Spi &_spi;
};


#endif /* PROGRAMMER_H_ */
