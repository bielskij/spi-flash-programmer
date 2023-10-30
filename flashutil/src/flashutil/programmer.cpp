/*
 * programmer.cpp
 *
 *  Created on: 11 wrz 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */
#include <algorithm>
#include <stdexcept>

#include "flashutil/programmer.h"
#include "flashutil/exception.h"
#include "flashutil/flash/builder.h"
#include "flashutil/debug.h"

// TODO: Each chip should define mask for those flags
#define STATUS_FLAG_WLE 0x02
#define STATUS_FLAG_WIP 0x01

#define ERASE_CHIP_TIMEOUT_MS    30000
#define ERASE_BLOCK_TIMEOUT_MS   10000
#define ERASE_SECTOR_TIMEOUT_MS    500
#define WRITE_STATUS_TIMEOUT_MS    200
#define WRITE_BYTE_TIMEOUT_MS      100
#define WRITE_PAGE_TIMEOUT_MS      200


Programmer::Programmer(Spi &spiDev, const FlashRegistry *registry) : _spi(spiDev) {
	this->_flashRegistry = registry;
}


Programmer::~Programmer() {
	this->end();
}


void Programmer::begin(const Flash *defaultGeometry) {
	auto &f = this->_flashInfo;

	TRACE("call, geometry: %p", defaultGeometry);

	this->_spi.attach();

	{
		std::vector<uint8_t> id;

		this->cmdGetInfo(id);

		f.setId(id);
		f.setName("Unknown chip");

		if (defaultGeometry != nullptr) {
			f.setGeometry(*defaultGeometry);
		}

		if (f.isIdValid()) {
			if (this->_flashRegistry != nullptr) {
				try {
					f = this->_flashRegistry->getById(id);
				} catch (const std::exception &) {}
			}

			if (f.isGeometryValid()) {
				INFO("Flash chip: %s (%02x, %02x, %02x), size: %zdB, blocks: %zd of %zdkB, sectors: %zd of %zdB",
					f.getName().c_str(), f.getId()[0], f.getId()[1], f.getId()[2],
					f.getSize(), f.getBlockCount(), f.getBlockSize() / 1024,
					f.getSectorCount(), f.getSectorSize()
				);

			} else {
				INFO("Detected flash chip of ID %02x, %02x, %02x - its geometry is unknown", id[0], id[1], id[2]);
			}

		} else {
			throw_Exception("No flash device detected!");
		}
	}
}


void Programmer::end() {
	TRACE(("call"));

	this->_spi.detach();

	this->_flashInfo = Flash();
}


const Flash &Programmer::getFlashInfo() const {
	return this->_flashInfo;
}


static void _verifyCommon(const Flash &info) {
	if (! info.isValid()) {
		throw std::runtime_error("Flash info is incomplete or invalid!");
	}
}


void Programmer::verifyFlashInfoAreaByAddress(uint32_t address, size_t size, size_t alignment) {
	_verifyCommon(this->_flashInfo);

	if (address + size > this->_flashInfo.getSize()) {
		throw std::runtime_error("Address is out of bound!");
	}

	if (alignment > 0) {
		if ((address % alignment) != 0) {
			throw std::runtime_error("Address is not a multiple of " + std::to_string(alignment) + "!");
		}
	}
}


void Programmer::verifyFlashInfoBlockNo(int blockNo) {
	_verifyCommon(this->_flashInfo);
}


void Programmer::verifyFlashInfoSectorNo(int sectorNo) {
	_verifyCommon(this->_flashInfo);
}


bool Programmer::checkErased(uint32_t address, size_t size) {
	bool ret = true;

	_verifyCommon(this->_flashInfo);

	{
		size_t  pageSize = this->_flashInfo.getPageSize();
		uint8_t pageBuffer[pageSize];

		if (size < pageSize) {
			size = pageSize;
		}

		this->cmdFlashReadBegin(address);

		for (int page = 0; page * pageSize < size / pageSize; page++) {
			Spi::Messages msgs;

			{
				auto &msg = msgs.add();

				msg.recv()
					.bytes(pageSize);

				msg.flags()
					.chipDeselect(false);
			}

			_spi.transfer(msgs);

			{
				auto *data = msgs.at(0).recv().data();

				ret = std::all_of(pageBuffer, pageBuffer + pageSize, [](uint8_t byte) { return byte == 0xff; });
				if (! ret) {
					break;
				}
			}
		}

		_spi.chipSelect(false);
	}

	return ret;
}


void Programmer::waitForWIPClearance(int timeoutMs) {
	bool wipOn = true;

	while (wipOn && (timeoutMs > 0)) {
		uint8_t statusReg;

		this->cmdGetStatus(statusReg);

		wipOn = ((statusReg & STATUS_FLAG_WIP) != 0);
		if (wipOn) {
			int interval = std::min(timeoutMs, 10);

			{
				struct timespec tv;

				tv.tv_sec  = (interval / 1000);
				tv.tv_nsec = (interval % 1000) * 1000000LL;

				nanosleep(&tv, nullptr);
			}

			timeoutMs -= interval;
		}
	}

	if (wipOn) {
		throw std::runtime_error("Waiting for WIP flag clearance has timed out!");
	}
}


void Programmer::unlockChip() {
	_verifyCommon(this->_flashInfo);

	if (this->_flashInfo.getProtectMask() == 0) {
		throw_Exception("Protect mask is note set!");
	}

	{
		uint8_t protectMask = this->_flashInfo.getProtectMask();
		uint8_t status;

		this->cmdGetStatus(status);

		if ((status & protectMask) != 0) {
			this->cmdWriteEnable();
			this->cmdWriteStatus(status & ~protectMask);

			this->waitForWIPClearance(WRITE_STATUS_TIMEOUT_MS);

			this->cmdGetStatus(status);

			if ((status & protectMask) != 0) {
				throw_Exception("Unable to unlock flash chip! Please check if WP pin is correctly polarized!");
			}
		}
	}
}


void Programmer::eraseChip() {
	TRACE(("call"));

	_verifyCommon(this->_flashInfo);

	this->cmdWriteEnable();
	this->cmdEraseChip();

	this->waitForWIPClearance(ERASE_CHIP_TIMEOUT_MS);
}


void Programmer::eraseBlockByAddress(uint32_t address, bool skipIfErased) {
	TRACE(("call"));

	this->verifyFlashInfoAreaByAddress(address, this->_flashInfo.getBlockSize(), this->_flashInfo.getBlockSize());

	this->cmdWriteEnable();
	this->cmdEraseBlock(address);

	this->waitForWIPClearance(ERASE_BLOCK_TIMEOUT_MS);
}


void Programmer::eraseBlockByNumber(int blockNo, bool skipIfErased) {
	TRACE(("call"));

	this->verifyFlashInfoBlockNo(blockNo);

	this->cmdWriteEnable();
	this->cmdEraseBlock(blockNo * this->_flashInfo.getBlockSize());

	this->waitForWIPClearance(ERASE_BLOCK_TIMEOUT_MS);
}


void Programmer::eraseSectorByAddress(uint32_t address, bool skipIfErased) {
	TRACE(("call"));

	this->verifyFlashInfoAreaByAddress(address, this->_flashInfo.getSectorSize(), this->_flashInfo.getSectorSize());

	this->cmdWriteEnable();
	this->cmdEraseSector(address);

	this->waitForWIPClearance(ERASE_SECTOR_TIMEOUT_MS);
}


void Programmer::eraseSectorByNumber(int sectorNo, bool skipIfErased) {
	TRACE(("call"));

	this->verifyFlashInfoSectorNo(sectorNo);

	this->cmdWriteEnable();
	this->cmdEraseSector(sectorNo * this->_flashInfo.getSectorSize());

	this->waitForWIPClearance(ERASE_SECTOR_TIMEOUT_MS);
}


void Programmer::cmdEraseChip() {
	Spi::Messages msgs;

	TRACE(("call"));

	{
		auto &msg = msgs.add();

		msg.send()
			.byte(0xc7); // Chip erase (CE)
	}

	_spi.transfer(msgs);
}


void Programmer::cmdEraseBlock(uint32_t address) {
	Spi::Messages msgs;

	TRACE(("call"));

	{
		auto &msg = msgs.add();

		msg.send()
			.byte(0xD8) // Block erase (BE)

			.byte((address >> 16) & 0xff)
			.byte((address >>  8) & 0xff)
			.byte((address >>  0) & 0xff);
	}

	_spi.transfer(msgs);
}


void Programmer::cmdEraseSector(uint32_t address) {
	Spi::Messages msgs;

	TRACE(("call"));

	{
		auto &msg = msgs.add();

		msg.send()
			.byte(0x20) // Sector erase (SE)

			.byte((address >> 16) & 0xff)
			.byte((address >>  8) & 0xff)
			.byte((address >>  0) & 0xff);
	}

	_spi.transfer(msgs);
}


void Programmer::cmdGetInfo(std::vector<uint8_t> &id) {
	Spi::Messages msgs;

	TRACE(("call"));

	id.clear();

	{
		auto &msg = msgs.add();

		msg.send()
			.byte(0x9f);

		msg.recv()
			.skip(1)
			.bytes(3);
	}

	_spi.transfer(msgs);

	{
		auto &recv = msgs.at(0).recv();

		id.push_back(recv.at(0));
		id.push_back(recv.at(1));
		id.push_back(recv.at(2));
	}
}


void Programmer::cmdGetStatus(uint8_t &reg) {
	Spi::Messages msgs;

	TRACE(("call"));

	{
		auto &msg = msgs.add();

		msg.send()
			.byte(0x05); // RDSR

		msg.recv()
			.skip(1)
			.bytes(1);
	}

	_spi.transfer(msgs);

	reg = msgs.at(0).recv().at(0);
}


void Programmer::cmdWriteEnable() {
	Spi::Messages msgs;

	TRACE(("call"));

	{
		auto &msg = msgs.add();

		msg.send()
			.byte(0x06); // WREN
	}

	_spi.transfer(msgs);
}


void Programmer::cmdWriteStatus(uint8_t reg) {
	Spi::Messages msgs;

	{
		auto &msg = msgs.add();

		msg.send()
			.byte(0x01) // WRSR
			.byte(reg);
	}

	_spi.transfer(msgs);
}


void Programmer::cmdFlashReadBegin(uint32_t address) {
	Spi::Messages msgs;

	TRACE(("call"));

	{
		auto &msg = msgs.add();

		msg.send()
			.byte(0x03) // Read data (READ)

			.byte((address >> 16) & 0xff) // Address
			.byte((address >>  8) & 0xff)
			.byte((address >>  0) & 0xff);

		msg.flags()
			.chipDeselect(false);
	}

	_spi.transfer(msgs);
}
