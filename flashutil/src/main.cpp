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

#include "crc8.h"
#include "protocol.h"
#include "spi.h"
#include "spi/creator.h"
#include "exception.h"
#include "debug.h"


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

	PRINTF(("Erasing block at address: %08x", address));

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

	PRINTF(("Erasing sector at address: %08x", address));

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


static void _flashWriteWait(SpiFlashStatus *status, int timeIntervalMs) {
	do {
		_flashCmdGetStatus(status);

		if (status->writeInProgress) {
			struct timespec tv;

			tv.tv_sec  = timeIntervalMs / 1000;
			tv.tv_nsec = timeIntervalMs * 1000000;

			nanosleep(&tv, NULL);
		}
	} while (status->writeInProgress);
}


#define PAGE_SIZE 256


static void _flashCmdPageWrite(uint32_t address, uint8_t *buffer, size_t bufferSize) {
	Spi::Messages msgs;

	PRINTF(("Writing page at address: %08x", address));

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


static void _flashRead(uint32_t address, uint8_t *buffer, size_t bufferSize, size_t *bufferWritten) {
	Spi::Messages msgs;

	DBG(("Reading %lu bytes from address: %08x", bufferSize, address));

	*bufferWritten = 0;

	{
		auto &msg = msgs.add();

		msg
			.reset()
			.autoChipSelect(false)
			.send()
				.byte(0x03) // Read data (READ)

				.byte((address >> 16) & 0xff) // Address
				.byte((address >>  8) & 0xff)
				.byte((address >>  0) & 0xff);

		spiDev->chipSelect(true);
		{
			spiDev->transfer(msgs);

			msg
				.reset()
				.autoChipSelect(false)
				.recv()
					.bytes(PAGE_SIZE);

			while (bufferSize > 0) {
				uint16_t toRead = bufferSize >= PAGE_SIZE ? PAGE_SIZE : PAGE_SIZE - bufferSize;
				uint16_t rest   = bufferSize - toRead;

				spiDev->transfer(msgs);

				memcpy(buffer + *bufferWritten, msgs.at(0).recv().data(), toRead);

				*bufferWritten = *bufferWritten + toRead;

				bufferSize -= toRead;
			}
		}
		spiDev->chipSelect(false);
	}
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

	Parameters() {
		this->outFd = nullptr;
		this->inFd  = nullptr;

		this->help        = false;
		this->baud        = 1000000;
		this->idx         = -1;
		this->read        = false;
		this->readBlock   = false;
		this->readSector  = false;
		this->erase       = false;
		this->eraseBlock  = false;
		this->eraseSector = false;
		this->write       = false;
		this->writeBlock  = false;
		this->writeSector = false;
		this->verify      = false;
		this->unprotect   = false;
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


static void _usage(const po::options_description &opts) {
	std::cout << opts << std::endl;

	exit(1);
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
				(OPT_BAUD,             po::value<int>(),         "Serial port baudrate")
				(OPT_READ        ",r",                           "Read to output file")
				(OPT_READ_BLOCK,       po::value<off_t>(),       "Read block at index")
				(OPT_READ_SECTOR,      po::value<off_t>(),       "Read Sector at index")
				(OPT_ERASE       ",e",                           "Erase whole chip")
				(OPT_ERASE_BLOCK,      po::value<off_t>(),       "Erase block at index")
				(OPT_ERASE_SECTOR,     po::value<off_t>(),       "Erase sector at index")
				(OPT_WRITE       ",w",                           "Write input file")
				(OPT_WRITE_BLOCK,                                "Write block from input file")
				(OPT_WRITE_SECTOR,                               "Write sector from input file")
				(OPT_VERIFY      ",V",                           "Verify writing process")
				(OPT_UNPROTECT   ",u",                           "Unprotect the chip before doing any operation on it")
				(OPT_FLASH_DESC  ",g",                           "Custom chip geometry in format <block_size>:<block_count>:<sector_size>:<sector_count>:<unprotect-mask-hex> (example: 65536:4:4096:64:8c)")
				;

			po::variables_map vm;

			po::store(po::command_line_parser(argc, argv).options(opDesc).run(), vm);

			if (vm.count(OPT_HELP)) {
				_usage(opDesc);
			}

			if (! vm.count(OPT_SERIAL)) {
				PRINTF(("Serial port was not provided!"));
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
					PRINTF(("Invalid flash-geometry syntax!"));

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

				PRINTF(("Flash chip: %s (%02x, %02x, %02x), size: %zdB, blocks: %zd of %zdkB, sectors: %zd of %zdkB",
					dev->name.c_str(), dev->id[0], dev->id[1], dev->id[2],
					dev->blockCount * dev->blockSize, dev->blockCount, dev->blockSize / 1024,
					dev->sectorCount, dev->sectorSize
				));

				_flashCmdGetStatus(&status);

				DBG(("status reg: %02x", status.raw));

				if ((status.raw & dev->protectMask) != 0) {
					PRINTF(("Flash is protected!"));
				}

				if (params.erase && params.eraseBlock) {
					params.eraseBlock = false;
				}

				if (params.unprotect) {
					if (dev->protectMask == 0) {
						PRINTF(("Unprotect requested but device do not support it!"));

					} else if ((dev->protectMask & status.raw) == 0) {
						PRINTF(("Chip is already unprotected!"));

					} else {
						PRINTF(("Unprotecting flash"));

						status.raw &= ~dev->protectMask;

						_flashCmdWriteEnable();
						_flashCmdWriteStatusRegister(&status);
						_flashWriteWait(&status, 100);

						if ((status.raw & dev->protectMask) != 0) {
							PRINTF(("Cannot unprotect the device!"));

							break;
						}

						PRINTF(("Flash unprotected"));
					}
				}

				if (params.erase) {
					for (int block = 0; block < dev->blockCount; block++) {
						_flashCmdWriteEnable();
						_flashCmdEraseBlock(block * dev->blockSize);
						_flashWriteWait(&status, 100);
					}
				}

				if (params.eraseBlock) {
					if (params.idx >= dev->blockCount) {
						throw_Exception("Block index is out of bounds! (" + std::to_string(params.idx) + " >= " + std::to_string(dev->blockCount) + ")");
					}

					_flashCmdWriteEnable();
					_flashCmdEraseBlock(params.idx * dev->blockSize);
					_flashWriteWait(&status, 100);
				}

				if (params.eraseSector) {
					if (params.idx >= dev->sectorCount) {
						throw_Exception("Sector index is out of bounds! (" + std::to_string(params.idx) + " >= " + std::to_string(dev->sectorCount) + ")");
					}

					_flashCmdWriteEnable();
					_flashCmdEraseSector(params.idx * dev->sectorSize);
					_flashWriteWait(&status, 100);
				}

				if (params.write) {
					std::array<uint8_t, PAGE_SIZE> pageBuffer;
					size_t  toWriteSize;
					uint32_t address = 0;

					if (! params.inFd) {
						PRINTF(("Writing requested but input file was not defined!"));

						ret = 1;
						break;
					}

					do {
						params.inFd->read(reinterpret_cast<char *>(pageBuffer.data()), pageBuffer.size());

						if (*params.inFd) {
							toWriteSize = pageBuffer.size();

						} else {
							toWriteSize = params.inFd->gcount();
						}

						if (toWriteSize > 0) {
							_flashCmdWriteEnable();
							_flashCmdPageWrite(address, pageBuffer.data(), toWriteSize);
							_flashWriteWait(&status, 100);

							address += toWriteSize;
						}
					} while (toWriteSize > 0);
				}

				if (params.read || params.readBlock || params.readSector) {
					if (! params.outFd) {
						PRINTF(("Reading requested but output file was not defined!"));

						ret = 1;
						break;
					}
				}

				if (params.read) {
					size_t   flashImageSize    = dev->blockSize * dev->blockCount;
					size_t   flashImageWritten = 0;

					std::unique_ptr<uint8_t[]> flashImage;

					if (flashImageSize == 0) {
						PRINTF(("Flash size is unknown!"));
						break;
					}

					PRINTF(("Reading whole flash"));

					flashImage.reset(new uint8_t[flashImageSize]);

					_flashRead(0, flashImage.get(), flashImageSize, &flashImageWritten);

					params.outFd->write(reinterpret_cast<char *>(flashImage.get()), flashImageWritten);

				} else if (params.readBlock) {
					uint8_t buffer[dev->blockSize];
					size_t  bufferWritten;

					if (params.idx >= dev->blockCount) {
						PRINTF(("Block index is out of bound! (%d >= %zd)", params.idx, dev->blockCount));

						ret = 1;
						break;
					}

					PRINTF(("Reading block %u (%#lx)", params.idx, params.idx * dev->blockSize));

					_flashRead(params.idx * dev->blockSize, buffer, dev->blockSize, &bufferWritten);

					params.outFd->write(reinterpret_cast<char *>(buffer), bufferWritten);

				} else if (params.readSector) {
					uint8_t buffer[dev->sectorSize];
					size_t  bufferWritten;

					if (params.idx >= dev->sectorCount) {
						PRINTF(("Sector index is out of bound! (%d >= %zd)", params.idx, dev->sectorCount));

						ret = 1;
						break;
					}

					PRINTF(("Reading sector %u (%#lx)", params.idx, params.idx * dev->sectorSize));

					_flashRead(params.idx * dev->sectorSize, buffer, dev->sectorSize, &bufferWritten);

					params.outFd->write(reinterpret_cast<char *>(buffer), bufferWritten);
				}

				if (params.verify) {
					uint8_t referenceBuffer[dev->blockSize];
					uint8_t buffer[dev->blockSize];
					size_t  bufferWritten;
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

						PRINTF(("Verifying block %d", blockNo));

						_flashRead(blockNo * dev->blockSize, buffer, compareSize, &bufferWritten);

						if (memcmp(buffer, referenceBuffer, bufferWritten) == 0) {
							PRINTF(("Verification of block %d -> SUCCESS", blockNo));

						} else {
							PRINTF(("Verification of block %d -> FAILED", blockNo));

							ret = 1;
							break;
						}

						blockNo++;
					}
				}
			}
		} catch (const std::exception &ex) {
			PRINTF(("Got exception: %s", ex.what()));

			ret = 1;
		}
	} while (0);

	return ret;
}
