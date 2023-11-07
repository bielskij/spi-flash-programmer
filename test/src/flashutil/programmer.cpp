#include <gtest/gtest.h>

#include "flashutil/spi/creator.h"
#include "flashutil/programmer.h"
#include "flashutil/entryPoint.h"
#include "flashutil/debug.h"

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

static std::unique_ptr<Serial> createSerial(Flash &info, const char *serialPath) {
	if (serialPath != nullptr) {
//		return std::make_unique<Serial>()
		return nullptr;

	} else {
		info.setId({ 0x01, 0x02, 0x03 });

		info.setBlockSize  (BLOCK_SIZE);
		info.setBlockCount (BLOCK_COUNT);
		info.setSectorSize (SECTOR_SIZE);
		info.setSectorCount(SECTOR_COUNT);
		info.setPageSize   (PAGE_SIZE);
		info.setPageCount  (PAGE_COUNT);

		info.setProtectMask(0x8c);

		return std::make_unique<SerialProgrammer>(info, PAYLOAD_SIZE);
	}
}


TEST(flashutil, programmer_connect) {
	Flash flashInfo;

	auto serial = createSerial(flashInfo, getenv("TEST_SERIAL"));

	std::unique_ptr<Spi> spi = SpiCreator().createSerialSpi(*serial.get());
	FlashRegistry registry;

	{
		std::vector<flashutil::EntryPoint::Parameters> operations;

		std::istringstream inStream;
		std::ostringstream outStream;

		{
			flashutil::EntryPoint::Parameters params;

			params.operation = flashutil::EntryPoint::Operation::NO_OPERATION;
			params.mode      = flashutil::EntryPoint::Mode::CHIP;
			params.flashInfo = &flashInfo;

			ASSERT_EQ(flashutil::EntryPoint::call(*spi.get(), registry, flashInfo, params), flashutil::EntryPoint::RC_SUCCESS);
		}

		{
			std::string data;

			for (int i = 0; i < 32; i++) {
				data.push_back(i);
			}

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
			params.index               = (flashInfo.getBlockSize() / flashInfo.getSectorSize()) + 1;
			params.omitRedundantWrites = true;
			params.verify              = true;

			params.inStream = &inStream;

			operations.push_back(params);
		}

		// Read
		{
			flashutil::EntryPoint::Parameters params;

			params.operation           = flashutil::EntryPoint::Operation::READ;
			params.mode                = flashutil::EntryPoint::Mode::SECTOR;
			params.index               = (flashInfo.getBlockSize() / flashInfo.getSectorSize()) + 1;

			params.outStream = &outStream;

			operations.push_back(params);
		}

		ASSERT_EQ(flashutil::EntryPoint::call(*spi.get(), registry, flashInfo, operations), flashutil::EntryPoint::RC_SUCCESS);

		ASSERT_EQ(inStream.str(), outStream.str());
	}
}
