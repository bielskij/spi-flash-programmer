#include <fstream>
#include <libgen.h>
#include <gtest/gtest.h>

#include "flashutil/spi/serial.h"
#include "flashutil/serial/hw.h"
#include "flashutil/programmer.h"
#include "flashutil/entryPoint.h"
#include "flashutil/flash/registry/reader/json.h"
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

#define PAYLOAD_SIZE 13


static FlashRegistry &getFlashRegistry() {
	static FlashRegistry registry;

	{
		FlashRegistryJsonReader reader;

		std::string path = __FILE__;

		path = path.substr(0, path.rfind('/'));

		std::ifstream stream(path + "/../../../flashutil/etc/chips.json");

		reader.read(stream, registry);
	}

	return registry;
}


static std::unique_ptr<Serial> createSerial(Flash &info) {
	const char *path = getenv("TEST_SERIAL_PATH");

	if (path != nullptr) {
		int baudInt = 115200;

		{
			const char *baud = getenv("TEST_SERIAL_BAUD");

			if (baud != nullptr) {
				baudInt = std::stoi(baud);
			}
		}

		return std::make_unique<HwSerial>(path, baudInt);

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


TEST(flashutil_entry_point, detect_flash) {
	Flash flashInfo;

	std::unique_ptr<Serial> serial = createSerial(flashInfo);
	std::unique_ptr<Spi>    spi    = std::make_unique<SerialSpi>(*serial.get());

	{
		flashutil::EntryPoint::Parameters params;

		params.operation = flashutil::EntryPoint::Operation::NO_OPERATION;
		params.mode      = flashutil::EntryPoint::Mode::CHIP;
		params.flashInfo = &flashInfo;

		flashutil::EntryPoint::call(*spi.get(), getFlashRegistry(), flashInfo, params);

		ASSERT_NE(flashInfo.getSize(), 0);
	}
}


TEST(flashutil_entry_point, write_erase_block) {
	Flash flashInfo;

	auto serial = createSerial(flashInfo);

	std::unique_ptr<Spi> spi = std::make_unique<SerialSpi>(*serial.get());

	debug_setLevel(DebugLevel::DEBUG_LEVEL_INFO);

	{
		std::stringstream  block1Stream;
		std::stringstream  block2Stream;
		std::stringstream  block3Stream;
		std::ostringstream blockReadStream;

		std::vector<flashutil::EntryPoint::Parameters> operations;

		{
			flashutil::EntryPoint::Parameters params;

			params.operation = flashutil::EntryPoint::Operation::NO_OPERATION;
			params.mode      = flashutil::EntryPoint::Mode::CHIP;
			params.flashInfo = &flashInfo;

			params.afterExecution = [&block1Stream, &block2Stream, &block3Stream](const flashutil::EntryPoint::Parameters &params) {
				size_t size = params.flashInfo->getBlockSize();

				for (int i = 0; i < size; i++) {
					block1Stream << (char)(i + 1);
					block2Stream << (char)(i + 2);
					block3Stream << (char)(i + 3);
				}
			};

			operations.push_back(params);
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
			params.omitRedundantWrites = true;
			params.verify              = true;

			params.index = 0;
			operations.push_back(params);

			params.index = 1;
			operations.push_back(params);

			params.index = 2;
			operations.push_back(params);
		}

		// Write
		{
			flashutil::EntryPoint::Parameters params;

			params.operation           = flashutil::EntryPoint::Operation::WRITE;
			params.mode                = flashutil::EntryPoint::Mode::BLOCK;
			params.omitRedundantWrites = true;
			params.verify              = true;

			params.index    = 0;
			params.inStream = &block1Stream;
			operations.push_back(params);

			params.index    = 1;
			params.inStream = &block2Stream;
			operations.push_back(params);

			params.index    = 2;
			params.inStream = &block3Stream;
			operations.push_back(params);
		}

		// Erase 1 in the middle
		{
			flashutil::EntryPoint::Parameters params;

			params.operation           = flashutil::EntryPoint::Operation::ERASE;
			params.mode                = flashutil::EntryPoint::Mode::BLOCK;
			params.omitRedundantWrites = true;
			params.verify              = true;

			params.index = 1;
			operations.push_back(params);
		}

		// Read
		{
			flashutil::EntryPoint::Parameters params;

			params.operation = flashutil::EntryPoint::Operation::READ;
			params.mode      = flashutil::EntryPoint::Mode::BLOCK;
			params.outStream = &blockReadStream;

			params.beforeExecution = [&blockReadStream](const flashutil::EntryPoint::Parameters &) {
				blockReadStream.str("");
			};

			{
				params.index          = 0;
				params.afterExecution = [&block1Stream, &blockReadStream](const flashutil::EntryPoint::Parameters &params) {
					ASSERT_EQ(block1Stream.str(), blockReadStream.str());
				};

				operations.push_back(params);
			}

			{
				params.index          = 2;
				params.afterExecution = [&block3Stream, &blockReadStream](const flashutil::EntryPoint::Parameters &params) {
					ASSERT_EQ(block3Stream.str(), blockReadStream.str());
				};

				operations.push_back(params);
			}

			{
				params.index          = 1;
				params.afterExecution = [&blockReadStream](const flashutil::EntryPoint::Parameters &params) {
					for (auto c : blockReadStream.str()) {
						ASSERT_EQ(c, (char)0xff);
					}
				};

				operations.push_back(params);
			}
		}

		flashutil::EntryPoint::call(*spi.get(), getFlashRegistry(), flashInfo, operations);
	}
}


TEST(flashutil_entry_point, write_program_whole) {
	Flash flashInfo;

	auto serial = createSerial(flashInfo);

	std::unique_ptr<Spi> spi = std::make_unique<SerialSpi>(*serial.get());

	size_t chipSize = 0;

	debug_setLevel(DebugLevel::DEBUG_LEVEL_INFO);

	{
		flashutil::EntryPoint::Parameters params;

		params.operation = flashutil::EntryPoint::Operation::NO_OPERATION;
		params.mode      = flashutil::EntryPoint::Mode::CHIP;
		params.flashInfo = &flashInfo;

		flashutil::EntryPoint::call(*spi.get(), getFlashRegistry(), flashInfo, params);

		chipSize = flashInfo.getSize();
	}

	ASSERT_GT(chipSize, 0);

	std::cout << "Testing on flash with size: " << std::to_string(chipSize) << std::endl;

	{
		std::vector<flashutil::EntryPoint::Parameters> operations;

		std::string       randomData;
		std::stringstream readData;

		for (size_t i = 0; i < chipSize; i++) {
			randomData.push_back(rand() % 0xff);
		}

		std::stringstream srcData(randomData);

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
			params.mode                = flashutil::EntryPoint::Mode::CHIP;
			params.omitRedundantWrites = false;
			params.verify              = true;

			operations.push_back(params);
		}

		// Write
		{
			flashutil::EntryPoint::Parameters params;

			params.operation           = flashutil::EntryPoint::Operation::WRITE;
			params.mode                = flashutil::EntryPoint::Mode::CHIP;
			params.omitRedundantWrites = false;
			params.verify              = true;
			params.inStream            = &srcData;
			params.index               = 0;

			operations.push_back(params);
		}

		// Read
		{
			flashutil::EntryPoint::Parameters params;

			params.operation = flashutil::EntryPoint::Operation::READ;
			params.mode      = flashutil::EntryPoint::Mode::CHIP;
			params.outStream = &readData;
			params.index     = 0;

			operations.push_back(params);
		}

		flashutil::EntryPoint::call(*spi.get(), getFlashRegistry(), flashInfo, operations);

		ASSERT_EQ(randomData, readData.str());
	}
}
