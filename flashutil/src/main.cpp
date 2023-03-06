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
#include "serial.h"
#include "debug.h"

#define TIMEOUT_MS 1000


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


static Serial *serial;


bool _cmdExecute(uint8_t cmd, uint8_t *data, size_t dataSize, uint8_t *response, size_t responseSize, size_t *responseWritten) {
	bool ret = true;

	do {
		if (responseWritten != NULL) {
			*responseWritten = 0;
		}

		// Send
		{
			uint16_t txDataSize = 4 + dataSize + 1;
			uint8_t txData[txDataSize];

			txData[0] = PROTO_SYNC_BYTE;
			txData[1] = cmd;
			txData[2] = dataSize >> 8;
			txData[3] = dataSize;

			if (dataSize) {
				memcpy(txData + 4, data, dataSize);
			}

			txData[txDataSize - 1] = crc8_get(txData, txDataSize - 1, PROTO_CRC8_POLY, PROTO_CRC8_START);

//			debug_dumpBuffer(txData, txDataSize, 32, 0);

			ret = serial->write(serial, txData, txDataSize, TIMEOUT_MS);
			if (! ret) {
				break;
			}
		}

		// Receive
		{
			uint8_t crc = PROTO_CRC8_START;
			uint8_t tmp;

			// sync
			{
				ret = serial->readByte(serial, &tmp, TIMEOUT_MS);
				if (! ret) {
					break;
				}

				ret = tmp == PROTO_SYNC_BYTE;
				if (! ret) {
					ERR(("Received byte is not a sync byte!"));
					break;
				}

				crc = crc8_getForByte(tmp, PROTO_CRC8_POLY, crc);
			}

			// code
			{
				ret = serial->readByte(serial, &tmp, TIMEOUT_MS);
				if (! ret) {
					break;
				}

				if (tmp != PROTO_NO_ERROR) {
					ERR(("Received error! (%#02x)", tmp));
				}

				crc = crc8_getForByte(tmp, PROTO_CRC8_POLY, crc);
			}

			// length
			{
				uint16_t dataLen = 0;

				for (int i = 0; i < 2; i++) {
					ret = serial->readByte(serial, &tmp, TIMEOUT_MS);
					if (! ret) {
						break;
					}

					dataLen |= (tmp << (8 * (1 - i)));

					crc = crc8_getForByte(tmp, PROTO_CRC8_POLY, crc);
				}

				if (! ret) {
					break;
				}

				// Data
				for (uint16_t i = 0; i < dataLen; i++) {
					ret = serial->readByte(serial, &tmp, TIMEOUT_MS);
					if (! ret) {
						break;
					}

					crc = crc8_getForByte(tmp, PROTO_CRC8_POLY, crc);

					if (i < responseSize) {
						if (response != NULL) {
							response[i] = tmp;
						}

						if (responseWritten != NULL) {
							*responseWritten += 1;
						}
					}
				}

				if (! ret) {
					break;
				}
			}

			// CRC
			{
				ret = serial->readByte(serial, &tmp, TIMEOUT_MS);
				if (! ret) {
					break;
				}

				ret = tmp == crc;
				if (! ret) {
					ERR(("CRC mismatch!"));
					break;
				}
			}
		}
	} while (0);

	return ret;
}


typedef struct _SpiFlashInfo {
	uint8_t manufacturerId;
	uint8_t deviceId[2];
} SpiFlashInfo;


static bool _spiCs(bool high) {
	return _cmdExecute(high ? PROTO_CMD_SPI_CS_HI : PROTO_CMD_SPI_CS_LO, NULL, 0, NULL, 0, NULL);
}


bool _spiTransfer(uint8_t *tx, uint16_t txSize, uint8_t *rx, uint16_t rxSize, size_t *rxWritten) {
	bool ret = true;

	do {
		uint16_t buffSize = 4 + txSize;
		uint8_t buff[buffSize];

		buff[0] = txSize >> 8;
		buff[1] = txSize;
		buff[2] = rxSize >> 8;
		buff[3] = rxSize;

		memcpy(&buff[4], tx, txSize);

		ret = _cmdExecute(PROTO_CMD_SPI_TRANSFER, buff, buffSize, rx, rxSize, rxWritten);
		if (! ret) {
			break;
		}
	} while (0);

	return ret;
}


bool _spiFlashGetInfo(SpiFlashInfo *info) {
	bool ret = true;

	ret = _spiCs(false);
	if (ret) {
		do {
			uint8_t tx[] = {
				0x9f // RDID
			};

			uint8_t rx[4];
			size_t rxWritten;

			memset(info, 0, sizeof(*info));

			ret = _spiTransfer(tx, sizeof(tx), rx, sizeof(rx), &rxWritten);
			if (! ret) {
				break;
			}

			info->manufacturerId = rx[1];
			info->deviceId[0]    = rx[2];
			info->deviceId[1]    = rx[3];
		} while (0);

		if (! _spiCs(true)) {
			ret = false;
		}
	}

	return ret;
}


typedef struct _SpiFlashStatus {
	bool writeEnableLatch;
	bool writeInProgress;

	uint8_t raw;
} SpiFlashStatus;

#define STATUS_FLAG_WLE 0x02
#define STATUS_FLAG_WIP 0x01

bool _spiFlashGetStatus(SpiFlashStatus *status) {
	bool ret = true;

	ret = _spiCs(false);
	if (ret) {
		do {
			uint8_t tx[] = {
				0x05 // RDSR
			};

			uint8_t rx[2];
			size_t rxWritten;

			memset(status, 0, sizeof(*status));

			ret = _spiTransfer(tx, sizeof(tx), rx, sizeof(rx), &rxWritten);
			if (! ret) {
				break;
			}

			status->raw = rx[1];

			status->writeEnableLatch = (status->raw & STATUS_FLAG_WLE) != 0;
			status->writeInProgress  = (status->raw & STATUS_FLAG_WIP) != 0;
		} while (0);

		if (! _spiCs(true)) {
			ret = false;
		}
	}

	return ret;
}


static bool _spiFlashWriteEnable() {
	bool ret = true;

	ret = _spiCs(false);
	if (ret) {
		do {
			uint8_t tx[] = {
				0x06 // WREN
			};

			ret = _spiTransfer(tx, sizeof(tx), NULL, 0, NULL);
			if (! ret) {
				break;
			}
		} while (0);

		if (! _spiCs(true)) {
			ret = false;
		}
	}

	return ret;
}


static bool _spiFlashWriteStatusRegister(SpiFlashStatus *status) {
	bool ret = true;

	ret = _spiCs(false);
	if (ret) {
		do {
			uint8_t tx[] = {
				0x01, // WRSR
				status->raw
			};

			ret = _spiTransfer(tx, sizeof(tx), NULL, 0, NULL);
			if (! ret) {
				break;
			}
		} while (0);

		if (! _spiCs(true)) {
			ret = false;
		}
	}

	return ret;
}

#if 0
static bool _spiFlashChipErase() {
	bool ret = true;

	ret = _spiCs(false);
	if (ret) {
		do {
			uint8_t tx[] = {
				0xC7 // CE
			};

			ret = _spiTransfer(tx, sizeof(tx), NULL, 0, NULL);
			if (! ret) {
				break;
			}
		} while (0);

		if (! _spiCs(true)) {
			ret = false;
		}
	}

	return ret;
}
#endif


static bool _spiFlashBlockErase(uint32_t address) {
	bool ret = true;

	PRINTF(("Erasign block at address: %08x", address));

	ret = _spiCs(false);
	if (ret) {
		do {
			uint8_t tx[] = {
				0xd8, // BE
				static_cast<uint8_t>((address >> 16) & 0xff),
				static_cast<uint8_t>((address >>  8) & 0xff),
				static_cast<uint8_t>((address >>  0) & 0xff)
			};

			ret = _spiTransfer(tx, sizeof(tx), NULL, 0, NULL);
			if (! ret) {
				break;
			}
		} while (0);

		if (! _spiCs(true)) {
			ret = false;
		}
	}

	return ret;
}


static bool _spiFlashWriteWait(SpiFlashStatus *status, int timeIntervalMs) {
	bool ret = true;

	do {
		ret = _spiFlashGetStatus(status);
		if (! ret) {
			break;
		}

		if (status->writeInProgress) {
			struct timespec tv;

			tv.tv_sec  = timeIntervalMs / 1000;
			tv.tv_nsec = timeIntervalMs * 1000000;

			nanosleep(&tv, NULL);
		}
	} while (status->writeInProgress);

	return ret;
}


#define PAGE_SIZE 256


bool _spiFlashPageWrite(uint32_t address, uint8_t *buffer, size_t bufferSize) {
	bool ret = true;

	PRINTF(("Writing page at address: %08x", address));

	ret = _spiCs(false);
	if (ret) {
		do {
			uint8_t tx[4 + bufferSize];

			tx[0] = 0x02; // PP
			tx[1] = (address >> 16) & 0xff;
			tx[2] = (address >>  8) & 0xff;
			tx[3] = (address >>  0) & 0xff;

			memcpy(tx + 4, buffer, bufferSize);

			ret = _spiTransfer(tx, 4 + bufferSize, NULL, 0, NULL);
			if (! ret) {
				break;
			}
		} while (0);

		if (! _spiCs(true)) {
			ret = false;
		}
	}

	return ret;
}


bool _spiFlashRead(uint32_t address, uint8_t *buffer, size_t bufferSize, size_t *bufferWritten) {
	bool ret = true;

	*bufferWritten = 0;

	DBG(("Reading %lu bytes from address: %08x", bufferSize, address));

	ret = _spiCs(false);
	if (ret) {
		do {
			{
				uint8_t tx[] = {
					0x03, // Read
					static_cast<uint8_t>((address >> 16) & 0xff), // Address
					static_cast<uint8_t>((address >>  8) & 0xff),
					static_cast<uint8_t>((address >>  0) & 0xff)
				};

				ret = _spiTransfer(tx, sizeof(tx), NULL, 0, NULL);
				if (! ret) {
					break;
				}
			}

			uint8_t rx[PAGE_SIZE];
			size_t rxWritten;

			while (bufferSize > 0) {
				uint16_t toRead = bufferSize >= PAGE_SIZE ? PAGE_SIZE : PAGE_SIZE - bufferSize;

				ret = _spiTransfer(NULL, 0, rx, toRead, &rxWritten);
				if (! ret) {
					break;
				}

				memcpy(buffer + *bufferWritten, rx, toRead);

				*bufferWritten = *bufferWritten + toRead;

				bufferSize -= toRead;
			}

			if (! ret) {
				break;
			}
		} while (0);

		if (! _spiCs(true)) {
			ret = false;
		}
	}

	return ret;
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
	bool write;
	bool verify;

	bool unprotect;

	Parameters() {
		this->outFd = nullptr;
		this->inFd  = nullptr;

		this->help       = false;
		this->baud       = 1000000;
		this->idx        = -1;
		this->read       = false;
		this->readBlock  = false;
		this->readSector = false;
		this->erase      = false;
		this->eraseBlock = false;
		this->write      = false;
		this->verify     = false;
		this->unprotect  = false;
	}
};



namespace po = boost::program_options;

#define OPT_VERBOSE "verbose"
#define OPT_HELP    "help"
#define OPT_SERIAL  "serial"
#define OPT_OUTPUT  "output"
#define OPT_INPUT   "input"
#define OPT_BAUD    "baud"

#define OPT_READ       "read"
#define OPT_READ_BLOCK "read-block"
#define OPT_READ_SECT  "read-sector"

#define OPT_ERASE       "erase"
#define OPT_ERASE_BLOCK "erase-block"
#define OPT_WRITE       "write"
#define OPT_VERIFY      "verify"
#define OPT_UNPROTECT   "unprotect"

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
				(OPT_SERIAL      ",p", po::value<std::string>(), "Serial port path")
				(OPT_OUTPUT      ",o", po::value<std::string>(), "Output file path or '-' for stdout")
				(OPT_INPUT       ",i", po::value<std::string>(), "Input file path or '-' for stdin")
				(OPT_BAUD        ",b", po::value<int>(),         "Serial port baudrate")
				(OPT_READ        ",R",                           "Read to output file")
				(OPT_READ_BLOCK  ",r", po::value<off_t>(),       "Read block at index")
				(OPT_READ_SECT   ",S", po::value<off_t>(),       "Read Sector at index")
				(OPT_ERASE       ",E",                           "Erase whole chip")
				(OPT_ERASE_BLOCK ",e", po::value<off_t>(),       "Erase block at index")
				(OPT_WRITE       ",W",                           "Write input file")
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
						PRINTF(("Unable to open output file! (%s)", output.c_str()));

						ret = 1;
						break;
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
						PRINTF(("Unable to open input file! (%s)", input.c_str()));

						ret = 1;
						break;
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

			if (vm.count(OPT_READ_SECT)) {
				params.readSector = true;
				params.idx        = vm[OPT_READ_SECT].as<off_t>();
			}

			if (vm.count(OPT_WRITE)) {
				params.write = true;
			}

			if (vm.count(OPT_ERASE)) {
				params.erase = true;
			}

			if (vm.count(OPT_ERASE_BLOCK)) {
				params.eraseBlock = true;
				params.idx        = vm[OPT_ERASE_BLOCK].as<off_t>();
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
					PRINTF(("Invalid flash-geometry, flash size calculated from blocks/sectors differs!"));
					exit(1);
				}
			}
		}

		serial = new_serial(params.serialPort.c_str(), params.baud);
		if (serial == NULL) {
			break;
		}

		{
			const SpiFlashDevice *dev = NULL;

			SpiFlashInfo info;
			SpiFlashStatus status;

			if (! _spiFlashGetInfo(&info)) {
				PRINTF(("Failure: Programmer is not responding!"));
				break;
			}

			if (
				((info.deviceId[0] == 0xff) && (info.deviceId[0] == 0xff) && (info.manufacturerId == 0xff)) ||
				((info.deviceId[0] == 0x00) && (info.deviceId[0] == 0x00) && (info.manufacturerId == 0x00))
			) {
				PRINTF(("No flash device detected!"));

				break;
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

			if (! _spiFlashGetStatus(&status)) {
				break;
			}

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

					if (! _spiFlashWriteEnable()) {
						break;
					}

					if (! _spiFlashWriteStatusRegister(&status)) {
						break;
					}

					if (! _spiFlashWriteWait(&status, 100)) {
						break;
					}

					if ((status.raw & dev->protectMask) != 0) {
						PRINTF(("Cannot unprotect the device!"));

						break;
					}

					PRINTF(("Flash unprotected"));
				}
			}

			if (params.erase) {
				for (int block = 0; block < dev->blockCount; block++) {
					if (! _spiFlashWriteEnable()) {
						ret = 1;
						break;
					}

					if (! _spiFlashBlockErase(block * dev->blockSize)) {
						ret = 1;
						break;
					}

					if (! _spiFlashWriteWait(&status, 100)) {
						ret = 1;
						break;
					}
				}
			}

			if (params.eraseBlock) {
				if (params.idx >= dev->blockCount) {
					PRINTF(("Block index is out of bound! (%d >= %zd)", params.idx, dev->blockCount));

					ret = 1;
					break;
				}

				if (! _spiFlashWriteEnable()) {
					ret = 1;
					break;
				}

				if (! _spiFlashBlockErase(params.idx * dev->blockSize)) {
					ret = 1;
					break;
				}

				if (! _spiFlashWriteWait(&status, 100)) {
					ret = 1;
					break;
				}
			}

			if (params.write) {
				std::array<uint8_t, PAGE_SIZE> pageBuffer;
				size_t  pageWritten;
				uint32_t address = 0;

				if (! params.inFd) {
					PRINTF(("Writing requested but input file was not defined!"));

					ret = 1;
					break;
				}

				do {
					params.inFd->read(reinterpret_cast<char *>(pageBuffer.data()), pageBuffer.size());

					if (*params.inFd) {
						pageWritten = pageBuffer.size();

					} else {
						pageWritten = params.inFd->gcount();
					}

					if (pageWritten > 0) {
						if (! _spiFlashWriteEnable()) {
							break;
						}

						if (! _spiFlashPageWrite(address, pageBuffer.data(), pageWritten)) {
							break;
						}

						if (! _spiFlashWriteWait(&status, 100)) {
							break;
						}

						address += pageWritten;
					}
				} while (pageWritten > 0);
			}

			if (params.read || params.readBlock || params.readSector) {
				if (! params.outpuFile.is_open()) {
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

				if (_spiFlashRead(0, flashImage.get(), flashImageSize, &flashImageWritten)) {
					params.outFd->write(reinterpret_cast<char *>(flashImage.get()), flashImageWritten);
				}

			} else if (params.readBlock) {
				uint8_t buffer[dev->blockSize];
				size_t  bufferWritten;

				if (params.idx >= dev->blockCount) {
					PRINTF(("Block index is out of bound! (%d >= %zd)", params.idx, dev->blockCount));

					ret = 1;
					break;
				}

				PRINTF(("Reading block %u (%#lx)", params.idx, params.idx * dev->blockSize));

				if (! _spiFlashRead(params.idx * dev->blockSize, buffer, dev->blockSize, &bufferWritten)) {
					break;
				}

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

				if (! _spiFlashRead(params.idx * dev->sectorSize, buffer, dev->sectorSize, &bufferWritten)) {
					break;
				}

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

					if (! _spiFlashRead(blockNo * dev->blockSize, buffer, compareSize, &bufferWritten)) {
						ret = 1;
						break;
					}

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
	} while (0);

	if (serial != NULL) {
		free(serial);
	}

	return ret;
}
