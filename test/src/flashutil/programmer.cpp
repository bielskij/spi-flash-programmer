#include <gtest/gtest.h>

#include "flashutil/spi/creator.h"
#include "flashutil/programmer.h"
#include "flashutil/entryPoint.h"

#include "serialFlash.h"

#define PAGE_SIZE        (16)
#define PAGE_COUNT       (32)
#define PAGES_PER_SECTOR  (2)

#define SECTOR_COUNT      (PAGE_COUNT / PAGES_PER_SECTOR)
#define SECTOR_SIZE       (PAGE_SIZE  * PAGES_PER_SECTOR)
#define SECTORS_PER_BLOCK (4)

#define BLOCK_COUNT  (SECTOR_COUNT / SECTORS_PER_BLOCK)
#define BLOCK_SIZE   (SECTOR_SIZE  * SECTORS_PER_BLOCK)

#define PAYLOAD_SIZE 128

static std::unique_ptr<SerialFlash> createSerial() {
	Flash flash;

	flash.setBlockSize  (BLOCK_SIZE);
	flash.setBlockCount (BLOCK_COUNT);
	flash.setSectorSize (SECTOR_SIZE);
	flash.setSectorCount(SECTOR_COUNT);
	flash.setPageSize   (PAGE_SIZE);
	flash.setPageCount  (PAGE_COUNT);

	flash.setProtectMask(0x8c);

	return std::make_unique<SerialFlash>(flash, PAYLOAD_SIZE);
}


TEST(flashutil, programmer_connect) {
	auto serial = createSerial();

	std::unique_ptr<Spi> spi = SpiCreator().createSerialSpi(*serial.get());
	FlashRegistry registry;

	{
		flashutil::EntryPoint::Parameters params;

		params.index             = 1;
		params.mode              = flashutil::EntryPoint::Mode::BLOCK;
		params.operation         = flashutil::EntryPoint::Operation::ERASE;
		params.unprotectIfNeeded = true;
		params.verify            = true;

		flashutil::EntryPoint::call(*spi.get(), registry, serial->getFlashInfo(), params);
	}
}
