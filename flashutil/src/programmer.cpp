/*
 * programmer.cpp
 *
 *  Created on: 11 wrz 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#include "programmer.h"
#include "flash/builder.h"

#include "debug.h"


#define KiB(_x)(1024 * _x)
#define Mib(_x)((1024 * 1024 * (_x)) / 8)


Programmer::Programmer(Spi *spiDev) {
	this->_spi              = spiDev;
	this->_maxReconnections = 3;
}


void Programmer::begin() {
	this->_flashInfo = Flash();

	{
		std::vector<uint8_t> id;

		this->cmdGetInfo(id);

		{
			auto &reg = this->getRegistry();

			try {
				auto &f = this->_flashInfo;

				f = reg.getById(id);

				PRINTFLN(("Flash chip: %s (%02x, %02x, %02x), size: %zdB, blocks: %zd of %zdkB, sectors: %zd of %zdB",
					f.getName().c_str(), f.getId()[0], f.getId()[1], f.getId()[2],
					f.getSize(), f.getBlockCount(), f.getBlockSize() / 1024,
					f.getSectorCount(), f.getSectorSize()
				));

			} catch (...) {
				PRINTF(("Detected flash chip of ID %02x, %02x, %02x - its geometry is unknown", id[0], id[1], id[2]));

				throw;
			}
		}
	}
}


void Programmer::end() {

}


void Programmer::eraseChip() {
	if (! this->_flashInfo.isValid()) {
		throw std::runtime_error("Flash info is incomplete or invalid!");
	}

	{
		Spi::Messages msgs;

		{
			auto &msg = msgs.add();

			msg.send()
				.byte(0xc7); // Chip erase (CE)
		}

		_spi->transfer(msgs);
	}
}


void Programmer::eraseBlockByAddress(uint32_t address) {
	if (! this->_flashInfo.isValid()) {
		throw std::runtime_error("Flash info is incomplete or invalid!");
	}

	if (address + this->_flashInfo.getBlockSize() > this->_flashInfo.getSize()) {
		throw std::runtime_error("Address is out of bound!");
	}

	if ((address % this->_flashInfo.getBlockSize()) != 0) {
		throw std::runtime_error("Address is not a multiple of block size!");
	}

	this->cmdEraseBlock(address);
}


void Programmer::eraseBlockByNumber(int blockNo) {
	this->cmdEraseBlock(blockNo * this->_flashInfo.getBlockSize());
}


void Programmer::eraseSectorByAddress(uint32_t address) {
	if (! this->_flashInfo.isValid()) {
		throw std::runtime_error("Flash info is incomplete or invalid!");
	}

	if (address + this->_flashInfo.getSectorSize() > this->_flashInfo.getSize()) {
		throw std::runtime_error("Address is out of bound!");
	}

	if ((address % this->_flashInfo.getSectorSize()) != 0) {
		throw std::runtime_error("Address is not a multiple of sector size!");
	}


	this->cmdEraseSector(address);
}


void Programmer::eraseSectorByNumber(int sectorNo) {
	this->cmdEraseSector(sectorNo * this->_flashInfo.getSectorSize());
}


void Programmer::cmdEraseBlock(uint32_t address) {
	Spi::Messages msgs;

	{
		auto &msg = msgs.add();

		msg.send()
			.byte(0xD8) // Block erase (BE)

			.byte((address >> 16) & 0xff)
			.byte((address >>  8) & 0xff)
			.byte((address >>  0) & 0xff);
	}

	_spi->transfer(msgs);
}


void Programmer::cmdEraseSector(uint32_t address) {
	Spi::Messages msgs;

	{
		auto &msg = msgs.add();

		msg.send()
			.byte(0x20) // Sector erase (SE)

			.byte((address >> 16) & 0xff)
			.byte((address >>  8) & 0xff)
			.byte((address >>  0) & 0xff);
	}

	_spi->transfer(msgs);
}


void Programmer::cmdGetInfo(std::vector<uint8_t> &id) {
	Spi::Messages msgs;

	id.clear();

	{
		auto &msg = msgs.add();

		msg.send()
			.byte(0x9f);

		msg.recv()
			.skip(1)
			.bytes(3);
	}

	_spi->transfer(msgs);

	{
		auto &recv = msgs.at(0).recv();

		id.push_back(recv.at(0));
		id.push_back(recv.at(1));
		id.push_back(recv.at(2));
	}
}


FlashRegistry &Programmer::getRegistry() {
	static FlashRegistry registry = []() {
		FlashRegistry reg;

		{
			FlashBuilder builder;

			reg.addFlash(builder
				.reset()
				.setName("Macronix MX25L2026E/MX25l2005A")
				.setJedecId    ({ 0xc2, 0x20, 0x12 })
				.setBlockSize  (KiB(64))
				.setSectorSize (KiB(4))
				.setSize       (Mib(2))
				.setProtectMask(0x8c)
				.build()
			);

			reg.addFlash(builder
				.reset()
				.setName("Macronix MX25V16066")
				.setJedecId    ({ 0xc2, 0x20, 0x15 })
				.setBlockSize  (KiB(64))
				.setSectorSize (KiB(4))
				.setSize       (Mib(16))
				.setProtectMask(0xbc)
				.build()
			);

			reg.addFlash(builder
				.reset()
				.setName("Winbond W25Q32")
				.setJedecId    ({ 0xef, 0x40, 0x16 })
				.setBlockSize  (KiB(64))
				.setSectorSize (KiB(4))
				.setSize       (Mib(32))
				.setProtectMask(0xfc)
				.build()
			);

			reg.addFlash(builder
				.reset()
				.setName("GigaDevice W25Q80")
				.setJedecId    ({ 0xc8, 0x40, 0x14 })
				.setBlockSize  (KiB(64))
				.setSectorSize (KiB(4))
				.setSize       (Mib(8))
				.setProtectMask(0x7c)
				.build()
			);
		}

		return reg;
	}();

	return registry;
}
