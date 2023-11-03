#include <gtest/gtest.h>

#include "flashutil/spi/creator.h"
#include "flashutil/programmer.h"
#include "flashutil/entryPoint.h"

#include "serialProgrammer.h"

#define PAGE_SIZE        (16)
#define PAGE_COUNT       (32)
#define PAGES_PER_SECTOR  (2)

#define SECTOR_COUNT      (PAGE_COUNT / PAGES_PER_SECTOR)
#define SECTOR_SIZE       (PAGE_SIZE  * PAGES_PER_SECTOR)
#define SECTORS_PER_BLOCK (4)

#define BLOCK_COUNT  (SECTOR_COUNT / SECTORS_PER_BLOCK)
#define BLOCK_SIZE   (SECTOR_SIZE  * SECTORS_PER_BLOCK)

#define PAYLOAD_SIZE 128

static std::unique_ptr<SerialProgrammer> createSerial() {
	Flash flash;

	flash.setId({ 0x01, 0x02, 0x03 });

	flash.setBlockSize  (BLOCK_SIZE);
	flash.setBlockCount (BLOCK_COUNT);
	flash.setSectorSize (SECTOR_SIZE);
	flash.setSectorCount(SECTOR_COUNT);
	flash.setPageSize   (PAGE_SIZE);
	flash.setPageCount  (PAGE_COUNT);

	flash.setProtectMask(0x8c);

	return std::make_unique<SerialProgrammer>(flash, PAYLOAD_SIZE);
}


TEST(flashutil, programmer_connect) {
	auto serial = createSerial();

	std::unique_ptr<Spi> spi = SpiCreator().createSerialSpi(*serial.get());
	FlashRegistry registry;

	{
		std::vector<flashutil::EntryPoint::Parameters> operations;

		std::istringstream inStream;

		{
			std::string data;

			data.append(32, 0);

			inStream.str(data);
		}

		// unlock
		{
			flashutil::EntryPoint::Parameters params;

			params.operation           = flashutil::EntryPoint::Operation::UNLOCK;
			params.mode                = flashutil::EntryPoint::Mode::CHIP;
			params.omitRedundantWrites = true;
			params.verify              = true;

			operations.push_back(params);
		}

		// Erase
		{
			flashutil::EntryPoint::Parameters params;

			params.operation           = flashutil::EntryPoint::Operation::ERASE;
			params.mode                = flashutil::EntryPoint::Mode::BLOCK;
			params.index               = 1;
			params.omitRedundantWrites = true;
			params.verify              = true;

			operations.push_back(params);
		}

		// Write
		{
			flashutil::EntryPoint::Parameters params;

			params.operation           = flashutil::EntryPoint::Operation::WRITE;
			params.mode                = flashutil::EntryPoint::Mode::SECTOR;
			params.index               = (serial->getFlashInfo().getBlockSize() / serial->getFlashInfo().getSectorSize()) + 1;
			params.omitRedundantWrites = true;
			params.verify              = true;

			params.inStream = &inStream;

			operations.push_back(params);
		}

		// Read
		{

		}

		flashutil::EntryPoint::call(*spi.get(), registry, serial->getFlashInfo(), operations);
	}
}
