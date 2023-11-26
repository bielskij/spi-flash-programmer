/*
 * programmer.cpp
 *
 *  Created on: 11 wrz 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */
#include <algorithm>
#include <stdexcept>
#include <cstring>

#include "flashutil/programmer.h"
#include "flashutil/exception.h"
#include "flashutil/flash/builder.h"
#include "flashutil/debug.h"

#define ERASE_CHIP_TIMEOUT_MS    30000
#define ERASE_BLOCK_TIMEOUT_MS   10000
#define ERASE_SECTOR_TIMEOUT_MS    500
#define WRITE_STATUS_TIMEOUT_MS    200
#define WRITE_BYTE_TIMEOUT_MS      100
#define WRITE_PAGE_TIMEOUT_MS      200


Programmer::Programmer(Spi &spiDev, const FlashRegistry *registry) : _spi(spiDev) {
	this->_flashRegistry = registry;
	this->_spiAttached   = false;
}


Programmer::~Programmer() {
	this->end();
}


void Programmer::begin(const Flash *defaultGeometry) {
	auto &f = this->_flashInfo;

	TRACE("call, geometry: %p", defaultGeometry);

	this->_spi.attach();
	this->_spiAttached = true;

	{
		std::vector<uint8_t> id;

		this->cmdGetInfo(id);

		f.setId(id);
		f.setPartNumber("Unknown chip");

		if (defaultGeometry != nullptr) {
			f.setGeometry(*defaultGeometry);
		}

		if (f.isIdValid()) {
			if (this->_flashRegistry != nullptr) {
				try {
					f = this->_flashRegistry->getById(id);
				} catch (const std::exception &) {}
			}

			if (! f.isGeometryValid()) {
				INFO("Detected flash chip of ID %02x, %02x, %02x - its geometry is unknown", id[0], id[1], id[2]);
			}

		} else {
			throw_Exception("No flash device detected!");
		}
	}
}


void Programmer::end() {
	TRACE(("call"));

	if (this->_spiAttached) {
		this->_spiAttached = false;
		this->_spi.detach();
	}

	this->_flashInfo = Flash();
}


const Flash &Programmer::getFlashInfo() const {
	return this->_flashInfo;
}


FlashStatus Programmer::getFlashStatus() {
	FlashStatus status;

	this->cmdGetStatus(status);

	return status;
}


FlashStatus Programmer::setFlashStatus(const FlashStatus &status) {
	this->cmdWriteEnable();
	this->cmdWriteStatus(status);

	return this->waitForWIPClearance(WRITE_STATUS_TIMEOUT_MS);
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
			throw std::runtime_error("Address " + std::to_string(address) + " is not a multiple of " + std::to_string(alignment) + "!");
		}
	}
}


void Programmer::verifyFlashInfoBlockNo(int blockNo) {
	_verifyCommon(this->_flashInfo);
}


void Programmer::verifyFlashInfoSectorNo(int sectorNo) {
	_verifyCommon(this->_flashInfo);
}


FlashStatus Programmer::waitForWIPClearance(int timeoutMs) {
	FlashStatus ret;

	ret.setBusy(true);

	while (ret.isWriteInProgress() && (timeoutMs > 0)) {
		uint8_t statusReg;

		this->cmdGetStatus(ret);

		if (ret.isWriteInProgress()) {
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

	if (ret.isWriteInProgress()) {
		throw std::runtime_error("Waiting for WIP flag clearance has timed out!");
	}

	return ret;
}


void Programmer::eraseChip() {
	TRACE(("call"));

	_verifyCommon(this->_flashInfo);

	this->cmdWriteEnable();
	this->cmdEraseChip();

	this->waitForWIPClearance(ERASE_CHIP_TIMEOUT_MS);
}


void Programmer::eraseBlockByAddress(uint32_t address) {
	TRACE("call");

	this->verifyFlashInfoAreaByAddress(address, this->_flashInfo.getBlockSize(), this->_flashInfo.getBlockSize());

	this->cmdWriteEnable();
	this->cmdEraseBlock(address);

	this->waitForWIPClearance(ERASE_BLOCK_TIMEOUT_MS);
}


void Programmer::eraseBlockByNumber(int blockNo) {
	this->eraseBlockByAddress(blockNo * this->_flashInfo.getBlockSize());
}


void Programmer::eraseSectorByAddress(uint32_t address) {
	TRACE("call");

	this->verifyFlashInfoAreaByAddress(address, this->_flashInfo.getSectorSize(), this->_flashInfo.getSectorSize());

	this->cmdWriteEnable();
	this->cmdEraseSector(address);

	this->waitForWIPClearance(ERASE_SECTOR_TIMEOUT_MS);
}


void Programmer::eraseSectorByNumber(int sectorNo) {
	this->eraseSectorByAddress(sectorNo * this->_flashInfo.getSectorSize());
}


void Programmer::writePage(uint32_t address, const std::vector<uint8_t> &page) {
	TRACE("call, address %08x, buffer: %p, size: %zd", address, page.data(), page.size());

	this->verifyFlashInfoAreaByAddress(address, page.size(), this->_flashInfo.getPageSize());

	this->cmdWriteEnable();
	this->cmdWritePage(address, page);

	this->waitForWIPClearance(ERASE_SECTOR_TIMEOUT_MS);
}


std::vector<uint8_t> Programmer::read(uint32_t address, size_t size) {
	this->verifyFlashInfoAreaByAddress(address, size, 1);

	TRACE("call, address %08x, size: %zd", address, size);

	this->cmdFlashReadBegin(address);

	{
		Spi::Messages msgs;

		{
			auto &msg = msgs.add();

			msg.recv()
				.bytes(size);
		}

		_spi.transfer(msgs);

		return std::move(msgs.at(0).recv().data());
	}
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

		id.push_back(recv.data().at(0));
		id.push_back(recv.data().at(1));
		id.push_back(recv.data().at(2));
	}
}


void Programmer::cmdGetStatus(FlashStatus &status) {
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

	status = FlashStatus(msgs.at(0).recv().data().at(0));
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


void Programmer::cmdWritePage(uint32_t address, const std::vector<uint8_t> &page) {
	Spi::Messages msgs;

	TRACE(("call"));

	{
		auto &msg = msgs.add();

		msg.send()
			.byte(0x02) // PP

			.byte((address >> 16) & 0xff)
			.byte((address >>  8) & 0xff)
			.byte((address >>  0) & 0xff)

			.data(page.data(), page.size());
	}

	_spi.transfer(msgs);
}


void Programmer::cmdWriteStatus(const FlashStatus &status) {
	Spi::Messages msgs;

	{
		auto &msg = msgs.add();

		msg.send()
			.byte(0x01) // WRSR
			.byte(status.getRegisterValue());
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
