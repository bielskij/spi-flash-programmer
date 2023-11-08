/*
 * programmer.h
 *
 *  Created on: 8 wrz 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef PROGRAMMER_H_
#define PROGRAMMER_H_

#include <vector>

#include "spi.h"
#include "flashutil/flash/registry.h"
#include "flashutil/flash/status.h"


class Programmer {
	public:
		Programmer(Spi &spiDev, const FlashRegistry *registry);
		virtual ~Programmer();

		void begin(const Flash *defaultGeometry);
		void end();

		void eraseChip();

		void eraseBlockByAddress(uint32_t address);
		void eraseBlockByNumber(int blockNo);

		void eraseSectorByAddress(uint32_t addres);
		void eraseSectorByNumber(int sectorNo);

		void writePage(uint32_t address, const std::vector<uint8_t> &page);
		std::vector<uint8_t> read(uint32_t address, size_t size);

		const Flash &getFlashInfo() const;

		FlashStatus getFlashStatus();
		FlashStatus setFlashStatus(const FlashStatus &status);

	private:
		void verifyFlashInfoAreaByAddress(uint32_t address, size_t size, size_t alignment);
		void verifyFlashInfoBlockNo(int blockNo);
		void verifyFlashInfoSectorNo(int sectorNo);

		FlashStatus waitForWIPClearance(int timeoutMs);

		void cmdEraseChip();
		void cmdEraseBlock(uint32_t address);
		void cmdEraseSector(uint32_t address);
		void cmdGetInfo(std::vector<uint8_t> &id);
		void cmdGetStatus(FlashStatus &status);
		void cmdWriteStatus(const FlashStatus &status);
		void cmdWriteEnable();
		void cmdWritePage(uint32_t address, const std::vector<uint8_t> &page);
		void cmdFlashReadBegin(uint32_t address);

	private:
		Flash                _flashInfo;
		const FlashRegistry *_flashRegistry;
		bool                 _spiAttached;

		Spi &_spi;
};


#endif /* PROGRAMMER_H_ */
