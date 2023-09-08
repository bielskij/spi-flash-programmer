/*
 * main.c
 *
 *  Created on: 27 lut 2022
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#include <string>
#include <array>
#include <memory>
#include <fstream>
#include <iostream>

#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>

#include <boost/program_options.hpp>

#include "spi.h"
#include "spi/creator.h"
#include "exception.h"
#include "debug.h"


#define WRITE_WAIT_TIMEOUT_MS 5000


typedef struct _SpiFlashDevice {
	std::string            name;
	std::array<uint8_t, 5> id;
	char                   idLen;
	size_t                 blockSize;
	size_t                 blockCount;
	size_t                 sectorSize;
	size_t                 sectorCount;
	size_t                 pageSize;
	uint8_t                protectMask;
} SpiFlashDevice;


#define INFO(_jedec_id, _ext_id, _block_size, _n_blocks, _sector_size, _n_sectors, _protectMask) \
		.id = {                                                        \
			((_jedec_id) >> 16) & 0xff,                                \
			((_jedec_id) >> 8) & 0xff,                                 \
			(_jedec_id) & 0xff,                                        \
			((_ext_id) >> 8) & 0xff,                                   \
			(_ext_id) & 0xff,                                          \
		},                                                             \
		.idLen       = (!(_jedec_id) ? 0 : (3 + ((_ext_id) ? 2 : 0))), \
		.blockSize   = (_block_size),                                  \
		.blockCount  = (_n_blocks),                                    \
		.sectorSize  = (_sector_size),                                 \
		.sectorCount = (_n_sectors),                                   \
		.pageSize    = 256,                                            \
		.protectMask = _protectMask,

#define KB(_x)(1024 * _x)

static const SpiFlashDevice flashDevices[] = {
	{
		"Macronix MX25L2026E/MX25l2005A",
		INFO(0xc22012, 0, KB(64), 4, KB(4), 64, 0x8c)
	},
	{
		"Macronix MX25V16066",
		INFO(0xc22015, 0, KB(64), 32, KB(4), 512, 0xbc)
	},
	{
		"Winbond W25Q32",
		INFO(0xef4016, 0, KB(64), 64, KB(4), 1024, 0xfc)
	},
	{
		"GigaDevice W25Q80",
		INFO(0xc84014, 0, KB(64), 16, KB(4), 255, 0x7c)
	}
};

static const int flashDevicesCount = sizeof(flashDevices) / sizeof(flashDevices[0]);


static SpiFlashDevice unknownDevice = {
	"Unknown SPI flash chip",

	INFO(0, 0, 0, 0, 0, 0, 0)
};


static std::unique_ptr<Spi> spiDev;


typedef struct _SpiFlashInfo {
	uint8_t manufacturerId;
	uint8_t deviceId[2];
} SpiFlashInfo;


static std::ifstream::pos_type _fileSize(const char* filename) {
	std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);

	return in.tellg();
}


static void _flashCmdGetInfo(SpiFlashInfo *info) {
	Spi::Messages msgs;

	{
		auto &msg = msgs.add();

		msg.send()
			.byte(0x9f);

		msg.recv()
			.skip(1)
			.bytes(3);
	}

	spiDev->transfer(msgs);

	{
		auto &recv = msgs.at(0).recv();

		info->manufacturerId = recv.at(0);
		info->deviceId[0]    = recv.at(1);
		info->deviceId[1]    = recv.at(2);
	}
}


typedef struct _SpiFlashStatus {
	bool writeEnableLatch;
	bool writeInProgress;

	uint8_t raw;
} SpiFlashStatus;

#define STATUS_FLAG_WLE 0x02
#define STATUS_FLAG_WIP 0x01

static void _flashCmdGetStatus(SpiFlashStatus *status) {
	Spi::Messages msgs;

	{
		auto &msg = msgs.add();

		msg.send()
			.byte(0x05); // RDSR

		msg.recv()
			.skip(1)
			.bytes(1);
	}

	spiDev->transfer(msgs);

	status->raw = msgs.at(0).recv().at(0);

	status->writeEnableLatch = (status->raw & STATUS_FLAG_WLE) != 0;
	status->writeInProgress  = (status->raw & STATUS_FLAG_WIP) != 0;
}


static void _flashCmdWriteEnable() {
	Spi::Messages msgs;

	{
		auto &msg = msgs.add();

		msg.send()
			.byte(0x06); // WREN
	}

	spiDev->transfer(msgs);
}


static void _flashCmdWriteStatusRegister(SpiFlashStatus *status) {
	Spi::Messages msgs;

	{
		auto &msg = msgs.add();

		msg.send()
			.byte(0x01) // WRSR
			.byte(status->raw);
	}

	spiDev->transfer(msgs);
}


static void _flashCmdEraseBlock(uint32_t address) {
	Spi::Messages msgs;

	{
		auto &msg = msgs.add();

		msg.send()
			.byte(0xD8) // Block erase (BE)

			.byte((address >> 16) & 0xff)
			.byte((address >>  8) & 0xff)
			.byte((address >>  0) & 0xff);
	}

	spiDev->transfer(msgs);
}


static void _flashCmdEraseSector(uint32_t address) {
	Spi::Messages msgs;

	{
		auto &msg = msgs.add();

		msg.send()
			.byte(0x20) // Sector erase (SE)

			.byte((address >> 16) & 0xff)
			.byte((address >>  8) & 0xff)
			.byte((address >>  0) & 0xff);
	}

	spiDev->transfer(msgs);
}


static void _flashWriteWait(int timeoutMs) {
	SpiFlashStatus status;

	do {
		int interval = std::min(timeoutMs, 10);

		_flashCmdGetStatus(&status);

		if (status.writeInProgress) {
			struct timespec tv;

			tv.tv_sec  = (interval / 1000);
			tv.tv_nsec = (interval % 1000) * 1000000LL;

			nanosleep(&tv, NULL);

			timeoutMs -= interval;
		}
	} while (status.writeInProgress && timeoutMs > 0);

	if (status.writeInProgress) {
		throw_Exception("Write wait timeout occurred!");
	}
}


#define PAGE_SIZE 256


static void _flashCmdPageWrite(uint32_t address, uint8_t *buffer, size_t bufferSize) {
	Spi::Messages msgs;

	{
		auto &msg = msgs.add();

		msg.send()
			.byte(0x02) // Page program (PP)

			.byte((address >> 16) & 0xff)
			.byte((address >>  8) & 0xff)
			.byte((address >>  0) & 0xff)

			.data(buffer, bufferSize);
	}

	spiDev->transfer(msgs);
}


struct Parameters {
	bool help;

	std::ofstream outpuFile;
	std::ifstream inputFile;
	std::string   serialPort;

	std::ostream *outFd;
	std::istream *inFd;

	int baud;
	int idx;

	bool read;
	bool readBlock;
	bool readSector;

	bool erase;
	bool eraseBlock;
	bool eraseSector;

	bool write;
	bool writeBlock;
	bool writeSector;

	bool verify;

	bool unprotect;
	bool omitRedundantOps;

	Parameters() {
		this->outFd = nullptr;
		this->inFd  = nullptr;

		this->help             = false;
		this->baud             = 500000;
		this->idx              = -1;
		this->read             = false;
		this->readBlock        = false;
		this->readSector       = false;
		this->erase            = false;
		this->eraseBlock       = false;
		this->eraseSector      = false;
		this->write            = false;
		this->writeBlock       = false;
		this->writeSector      = false;
		this->verify           = false;
		this->unprotect        = false;
		this->omitRedundantOps = false;
	}
};



namespace po = boost::program_options;

#define OPT_VERBOSE "verbose"
#define OPT_HELP    "help"
#define OPT_SERIAL  "serial"
#define OPT_OUTPUT  "output"
#define OPT_INPUT   "input"
#define OPT_BAUD    "serial-baud"

#define OPT_READ       "read"
#define OPT_READ_BLOCK "read-block"
#define OPT_READ_SECTOR  "read-sector"

#define OPT_ERASE        "erase"
#define OPT_ERASE_BLOCK  "erase-block"
#define OPT_ERASE_SECTOR "erase-sector"

#define OPT_WRITE        "write"
#define OPT_WRITE_BLOCK  "write-block"
#define OPT_WRITE_SECTOR "write-sector"

#define OPT_VERIFY       "verify"
#define OPT_UNPROTECT    "unprotect"

#define OPT_FLASH_DESC "flash-geometry"
#define OPT_OMIT_REDUNDANT_OPS "no-redudant-cycles"


static void _usage(const po::options_description &opts) {
	std::cout << opts << std::endl;

	exit(1);
}


class ChipSelector {
	public:
		ChipSelector(Spi &spi) : _spi(spi) {
			_spi.chipSelect(true);
		}

		~ChipSelector() {
			_spi.chipSelect(false);
		}

	private:
		Spi &_spi;
};


static void _flashRead(uint32_t address, const SpiFlashDevice &dev, uint8_t *buffer, size_t bufferSize) {
	int defaultRetransmissions = 3;//spiDev->getConfig().getRetransmissions();

	size_t bufferWritten = 0;
	int    tries = 0;

	DBG(("Reading %lu bytes from address: %08x", bufferSize, address));

	do {
		ChipSelector selector(*spiDev.get());

		// Set reading address
		{
			Spi::Messages msgs;

			{
				auto &msg = msgs.add();

				msg
					.reset()
					.send()
						.byte(0x03) // Read data (READ)

						.byte((address >> 16) & 0xff) // Address
						.byte((address >>  8) & 0xff)
						.byte((address >>  0) & 0xff);

				msg.
					flags()
						.chipDeselect(false);
			}

			spiDev->transfer(msgs);
		}

		try {
			Spi::Messages msgs;

			msgs.add()
				.flags()
					.chipDeselect(false);

			// Disable retransmissions
			{
				Spi::Config c = spiDev->getConfig();

//				c.setRetransmissions(0);

				spiDev->setConfig(c);
			}

			while (bufferSize > 0) {
				size_t toRead = bufferSize >= dev.pageSize ? dev.pageSize : bufferSize;

				msgs.at(0)
					.recv()
						.reset()
						.bytes(toRead);

				spiDev->transfer(msgs);

				memcpy(buffer + bufferWritten, msgs.at(0).recv().data(), toRead);

				bufferWritten += toRead;
				bufferSize    -= toRead;
				address       += toRead;

				tries = 0;
			}
		} catch(const Exception &) {
			if (++tries >= defaultRetransmissions) {
				throw;
			}
		}
	} while (bufferSize > 0);

	// Restore retransmissions
	{
		Spi::Config c = spiDev->getConfig();

//		c.setRetransmissions(defaultRetransmissions);

		spiDev->setConfig(c);
	}
}


static void _doErase(int idx, const SpiFlashDevice &dev, bool block, bool noRedundantCycles) {
	size_t   eraseSize = block ? dev.blockSize : dev.sectorSize;
	uint32_t address   = idx * (block ? dev.blockSize : dev.sectorSize);

	bool needErase = ! noRedundantCycles;

	if (! needErase) {
		uint8_t pageBuffer[dev.pageSize];

		for (int page = 0; page < eraseSize / dev.pageSize; page++) {
			_flashRead(address + page * dev.pageSize, dev, pageBuffer, dev.pageSize);

			for (int i = 0; i < dev.pageSize; i++) {
				if (pageBuffer[i] != 0xff) {
					needErase = true;
					break;
				}
			}

			if (needErase) {
				break;
			}
		}
	}

	PRINTF(("Erasing %s %u (%08x): ", block ? "block" : "sector", idx, address));

	if (! needErase) {
		PRINTFLN(("SKIPPED (already erased)"));

	} else {
		_flashCmdWriteEnable();
		if (block) {
			_flashCmdEraseBlock(address);
		} else {
			_flashCmdEraseSector(address);
		}
		_flashWriteWait(WRITE_WAIT_TIMEOUT_MS);

		PRINTFLN(("DONE"));
	}
}


static void _doReadToStream(uint32_t address, size_t size, const SpiFlashDevice &dev, std::ostream &stream) {
	if (address + size > dev.blockCount * dev.blockSize) {
		throw_Exception("Area to read is located out of device bounds!");
	}

	std::unique_ptr<uint8_t[]> flashImage;

	flashImage.reset(new uint8_t[size]);

	_flashRead(address, dev, flashImage.get(), size);

	stream.write(reinterpret_cast<char *>(flashImage.get()), size);
}


static void _doWriteFromStream(uint32_t address, size_t size, const SpiFlashDevice &dev, std::istream &stream, bool noRedundantCycles) {
	size_t written = 0;

	if (address + size > dev.blockCount * dev.blockSize) {
		throw_Exception("Area to write is located out of device bounds!");
	}

	if ((address % dev.pageSize) != 0) {
		throw_Exception("Address is not page aligned!");
	}

	while (size != written && ! stream.eof()) {
		char pageBuffer[dev.pageSize];

		size_t toRead = std::min(size - written, dev.pageSize);
		size_t read = 0;

		memset(pageBuffer, 0xff, dev.pageSize);

		stream.read(pageBuffer, toRead);
		read = stream.gcount();

		if (read) {
			bool needWrite = ! noRedundantCycles;

			if (! needWrite) {
				uint8_t flashBuffer[dev.pageSize];

				_flashRead(address, dev, flashBuffer, read);

				if (memcmp(flashBuffer, pageBuffer, read) != 0) {
					needWrite = true;
				}
			}

			uint32_t pageIdx = address / dev.pageSize;

			PRINTF(("Writing page %u, in sector: %zd, in block %zd (%08x): ",
				pageIdx, (pageIdx * dev.pageSize) / dev.sectorSize, (pageIdx * dev.pageSize) / dev.blockSize, address
			));

			if (! needWrite) {
				PRINTFLN(("SKIPPED (the same)"));

			} else {
				_flashCmdWriteEnable();
				_flashCmdPageWrite(address, (uint8_t *) pageBuffer, dev.pageSize);
				_flashWriteWait(WRITE_WAIT_TIMEOUT_MS);

				PRINTFLN(("DONE"));
			}

			address += dev.pageSize;
			written += dev.pageSize;
		}
	}
}


int main(int argc, char *argv[]) {
	int ret = 0;

	do {
		Parameters params;

		{
			po::options_description opDesc("Program parameters");

			opDesc.add_options()
				(OPT_VERBOSE     ",v",                           "Verbose output")
				(OPT_HELP        ",h",                           "Print usage message")
				(OPT_SERIAL      ",s", po::value<std::string>(), "Serial port path")
				(OPT_OUTPUT      ",o", po::value<std::string>(), "Output file path or '-' for stdout")
				(OPT_INPUT       ",i", po::value<std::string>(), "Input file path or '-' for stdin")
				(OPT_READ        ",r",                           "Read to output file")
				(OPT_WRITE       ",w",                           "Write input file")
				(OPT_ERASE       ",e",                           "Erase whole chip")
				(OPT_VERIFY      ",V",                           "Verify writing process")
				(OPT_UNPROTECT   ",u",                           "Unprotect the chip before doing any operation on it")
				(OPT_FLASH_DESC  ",g",                           "Custom chip geometry in format <block_size>:<block_count>:<sector_size>:<sector_count>:<unprotect-mask-hex> (example: 65536:4:4096:64:8c)")
				(OPT_BAUD,             po::value<int>(),         "Serial port baudrate")
				(OPT_READ_BLOCK,       po::value<off_t>(),       "Read block at index")
				(OPT_READ_SECTOR,      po::value<off_t>(),       "Read Sector at index")
				(OPT_ERASE_BLOCK,      po::value<off_t>(),       "Erase block at index")
				(OPT_ERASE_SECTOR,     po::value<off_t>(),       "Erase sector at index")
				(OPT_WRITE_BLOCK,      po::value<off_t>(),       "Write block from input file")
				(OPT_WRITE_SECTOR,     po::value<off_t>(),       "Write sector from input file")
				(OPT_OMIT_REDUNDANT_OPS,                         "Prevent from redundant erase/write cycles");
				;

			po::variables_map vm;

			po::store(po::command_line_parser(argc, argv).options(opDesc).run(), vm);

			if (vm.count(OPT_HELP)) {
				_usage(opDesc);
			}

			if (! vm.count(OPT_SERIAL)) {
				PRINTFLN(("Serial port was not provided!"));
				_usage(opDesc);

			} else {
				params.serialPort = vm[OPT_SERIAL].as<std::string>();
			}

			if (vm.count(OPT_BAUD)) {
				params.baud = vm[OPT_BAUD].as<int>();
			}

			if (vm.count(OPT_OUTPUT)) {
				auto output = vm[OPT_OUTPUT].as<std::string>();
				if (output == "-") {
					params.outFd = &std::cout;

				} else {
					params.outpuFile.open(output, std::ios::out | std::ios::trunc | std::ios::binary);
					if (! params.outpuFile.is_open()) {
						throw_Exception("Unable to open output file! (" + output + ")");
					}

					params.outFd = &params.outpuFile;
				}
			}

			if (vm.count(OPT_INPUT)) {
				auto input = vm[OPT_INPUT].as<std::string>();
				if (input == "-") {
					params.inFd = &std::cin;

				}
				else {
					params.inputFile.open(input, std::ios::in | std::ios::binary);
					if (! params.inputFile.is_open()) {
						throw_Exception("Unable to open input file! (" + input + ")");
					}

					params.inFd = &params.inputFile;
				}
			}

			if (vm.count(OPT_READ)) {
				params.read = true;
			}

			if (vm.count(OPT_READ_BLOCK)) {
				params.readBlock = true;
				params.idx       = vm[OPT_READ_BLOCK].as<off_t>();
			}

			if (vm.count(OPT_READ_SECTOR)) {
				params.readSector = true;
				params.idx        = vm[OPT_READ_SECTOR].as<off_t>();
			}

			if (vm.count(OPT_WRITE)) {
				params.write = true;
			}

			if (vm.count(OPT_WRITE_BLOCK)) {
				params.writeBlock = true;
				params.idx        = vm[OPT_WRITE_BLOCK].as<off_t>();
			}

			if (vm.count(OPT_WRITE_SECTOR)) {
				params.writeSector = true;
				params.idx         = vm[OPT_WRITE_SECTOR].as<off_t>();
			}

			if (vm.count(OPT_ERASE)) {
				params.erase = true;
			}

			if (vm.count(OPT_ERASE_BLOCK)) {
				params.eraseBlock = true;
				params.idx        = vm[OPT_ERASE_BLOCK].as<off_t>();
			}

			if (vm.count(OPT_ERASE_SECTOR)) {
				params.eraseSector = true;
				params.idx         = vm[OPT_ERASE_SECTOR].as<off_t>();
			}

			if (vm.count(OPT_VERIFY)) {
				params.verify = true;
			}

			if (vm.count(OPT_UNPROTECT)) {
				params.unprotect = true;
			}

			if (vm.count(OPT_OMIT_REDUNDANT_OPS)) {
				params.omitRedundantOps = true;
			}

			if (vm.count(OPT_FLASH_DESC)) {
				auto desc = vm[OPT_FLASH_DESC].as<std::string>();

				int blockCount;
				int blockSize;
				int sectorCount;
				int sectorSize;
				uint8_t unprotectMask;

				if (sscanf(
					desc.c_str(), "%d:%d:%d:%d:%02hhx",
					&blockSize, &blockCount,
					&sectorSize, &sectorCount,
					&unprotectMask
				) != 5
				) {
					PRINTFLN(("Invalid flash-geometry syntax!"));

					_usage(opDesc);
				}

				unknownDevice.blockCount  = blockCount;
				unknownDevice.blockSize   = blockSize;
				unknownDevice.sectorCount = sectorCount;
				unknownDevice.sectorSize  = sectorSize;
				unknownDevice.protectMask = unprotectMask;

				if (blockCount * blockSize != sectorCount * sectorSize) {
					throw_Exception("Invalid flash-geometry, flash size calculated from blocks/sectors differs!");
				}
			}
		}

		try {
			{
				SpiCreator creator;

				spiDev = creator.createSerialSpi(params.serialPort, params.baud);
			}

			{
				const SpiFlashDevice *dev = NULL;

				SpiFlashInfo info;
				SpiFlashStatus status;

				_flashCmdGetInfo(&info);

				if (
					((info.deviceId[0] == 0xff) && (info.deviceId[0] == 0xff) && (info.manufacturerId == 0xff)) ||
					((info.deviceId[0] == 0x00) && (info.deviceId[0] == 0x00) && (info.manufacturerId == 0x00))
				) {
					throw_Exception("No flash device detected!");
				}

				for (int i = 0; i < flashDevicesCount; i++) {
					const SpiFlashDevice *ip = &flashDevices[i];

					if (
						(ip->id[0] == info.manufacturerId) &&
						(ip->id[1] == info.deviceId[0]) &&
						(ip->id[2] == info.deviceId[1])
					) {
						dev = ip;
						break;
					}
				}

				if (dev == NULL) {
					ERR(("Unrecognized SPI flash device!"));

					unknownDevice.id[0] = info.manufacturerId;
					unknownDevice.id[1] = info.deviceId[0];
					unknownDevice.id[2] = info.deviceId[1];

					dev = &unknownDevice;
				}

				PRINTFLN(("Flash chip: %s (%02x, %02x, %02x), size: %zdB, blocks: %zd of %zdkB, sectors: %zd of %zdB",
					dev->name.c_str(), dev->id[0], dev->id[1], dev->id[2],
					dev->blockCount * dev->blockSize, dev->blockCount, dev->blockSize / 1024,
					dev->sectorCount, dev->sectorSize
				));

				_flashCmdGetStatus(&status);

				if ((status.raw & dev->protectMask) != 0) {
					PRINTFLN(("Flash is protected!"));
				}

				if (params.erase && params.eraseBlock) {
					params.eraseBlock = false;
				}

				if (params.unprotect) {
					if (dev->protectMask == 0) {
						PRINTFLN(("Unprotect requested but device do not support it!"));

					} else if ((dev->protectMask & status.raw) == 0) {
						PRINTFLN(("Chip is already unprotected!"));

					} else {
						uint8_t origRaw = status.raw;

						PRINTFLN(("Unprotecting flash"));

						status.raw &= ~dev->protectMask;

						if (status.raw != origRaw) {
							_flashCmdWriteEnable();
							_flashCmdWriteStatusRegister(&status);
							_flashWriteWait(WRITE_WAIT_TIMEOUT_MS);

							if ((status.raw & dev->protectMask) != 0) {
								PRINTFLN(("Cannot unprotect the device!"));

								break;
							}
						}

						PRINTFLN(("Flash unprotected"));
					}
				}

				if (params.erase) {
					for (int block = 0; block < dev->blockCount; block++) {
						_doErase(block, *dev, true, params.omitRedundantOps);
					}
				}

				if (params.eraseBlock) {
					if (params.idx >= dev->blockCount) {
						throw_Exception("Block index is out of bounds! (" + std::to_string(params.idx) + " >= " + std::to_string(dev->blockCount) + ")");
					}

					_doErase(params.idx, *dev, true, params.omitRedundantOps);
				}

				if (params.eraseSector) {
					if (params.idx >= dev->sectorCount) {
						throw_Exception("Sector index is out of bounds! (" + std::to_string(params.idx) + " >= " + std::to_string(dev->sectorCount) + ")");
					}

					_doErase(params.idx, *dev, false, params.omitRedundantOps);
				}

				if (params.write) {
					_doWriteFromStream(0, dev->blockCount * dev->blockSize, *dev, *params.inFd, params.omitRedundantOps);

				} else if (params.writeBlock) {
					_doWriteFromStream(params.idx * dev->blockSize, dev->blockSize, *dev, *params.inFd, params.omitRedundantOps);

				} else if (params.writeSector) {
					_doWriteFromStream(params.idx * dev->sectorSize, dev->sectorSize, *dev, *params.inFd, params.omitRedundantOps);
				}

				if (params.read || params.readBlock || params.readSector) {
					if (! params.outFd) {
						PRINTFLN(("Reading requested but output file was not defined!"));

						ret = 1;
						break;
					}
				}

				if (params.read) {
					PRINTFLN(("Reading whole flash"));

					_doReadToStream(0, dev->blockCount * dev->blockSize, *dev, *params.outFd);

				} else if (params.readBlock) {
					PRINTFLN(("Reading block %u (%#lx)", params.idx, params.idx * dev->blockSize));

					_doReadToStream(params.idx * dev->blockSize, dev->blockSize, *dev, *params.outFd);

				} else if (params.readSector) {
					PRINTFLN(("Reading sector %u (%#lx)", params.idx, params.idx * dev->sectorSize));

					_doReadToStream(params.idx * dev->sectorSize, dev->sectorSize, *dev, *params.outFd);
				}

				if (params.verify) {
					uint8_t referenceBuffer[dev->blockSize];
					uint8_t buffer[dev->blockSize];
					int     blockNo = 0;

					if (params.write) {
						params.inFd->clear();
						params.inFd->seekg(0, std::ios::beg);

					} else {
						memset(referenceBuffer, 0xff, dev->blockSize);
					}

					while (blockNo < dev->blockCount) {
						size_t compareSize = 0;

						if (params.inFd) {
							params.inFd->read(reinterpret_cast<char *>(referenceBuffer), dev->blockSize);

							if (*params.inFd) {
								compareSize = dev->blockSize;

							} else {
								compareSize = params.inFd->gcount();
							}
						}

						if (compareSize <= 0) {
							break;
						}

						PRINTF(("Verifying block %d: ", blockNo));

						_flashRead(blockNo * dev->blockSize, *dev, buffer, compareSize);

						if (memcmp(buffer, referenceBuffer, compareSize) == 0) {
							PRINTFLN(("DONE"));

						} else {
							PRINTFLN(("FAILED"));

							ret = 1;
							break;
						}

						blockNo++;
					}
				}
			}
		} catch (const std::exception &ex) {
			PRINTFLN(("Got exception: %s", ex.what()));

			ret = 1;
		}
	} while (0);

	return ret;
}
