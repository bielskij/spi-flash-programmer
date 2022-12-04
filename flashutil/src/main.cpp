/*
 * main.c
 *
 *  Created on: 27 lut 2022
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#include <memory>
#include <string>
#include <iostream>

#include <cinttypes>
#include <cstdio>
#include <cstdlib>

#if defined(__unix__)
#include <unistd.h>
#endif

#include <boost/program_options.hpp>
#include <boost/chrono.hpp>
#include <boost/thread.hpp>

#include "crc8.h"
#include "protocol.h"
#include "serial.h"
#include "debug.h"

#define TIMEOUT_MS 1000


struct SpiFlashDevice {
	std::string name;
	uint8_t id[5];
	char    idLen;
	size_t  blockSize;
	size_t  blockCount;
	size_t  sectorSize;
	size_t  sectorCount;
	size_t  pageSize;
	uint8_t protectMask;

	SpiFlashDevice(const std::string &name, uint32_t jedecId, uint16_t extId, size_t blockSize, size_t blockCount, size_t sectorSize, size_t sectorCount, uint8_t protectMask) : name(name) {
		this->id[0] = (jedecId >> 16) & 0xff;
		this->id[1] = (jedecId >>  8) & 0xff;
		this->id[2] = (jedecId >>  0) & 0xff;
		this->id[3] = (extId >> 8) & 0xff;
		this->id[4] = (extId >> 0) & 0xff;

		if (jedecId == 0) {
			this->idLen = 0;
		
		} else {
			this->idLen = 3;

			if (extId != 0) {
				this->idLen += 2;
			}
		}

		this->blockSize   = blockSize;
		this->blockCount  = blockCount;
		this->sectorSize  = sectorSize;
		this->sectorCount = sectorCount;
		this->protectMask = protectMask;
	};
};

#define KB(_x)(1024 * _x)

static const std::vector<SpiFlashDevice> flashDevices = {
	SpiFlashDevice(
		"Macronix MX25L2026E/MX25l2005A",
		0xc22012, 0, KB(64), 4, KB(4), 64, 0x8c
	),
	SpiFlashDevice(
		"Macronix MX25V16066",
		0xc22015, 0, KB(64), 32, KB(4), 512, 0xbc
	),
	SpiFlashDevice(
		"Winbond W25Q32",
		0xef4016, 0, KB(64), 64, KB(4), 1024, 0xfc
	),
	SpiFlashDevice(
		"GigaDevice W25Q80",
		0xc84014, 0, KB(64), 16, KB(4), 255, 0x7c
	)
};

static SpiFlashDevice unknownDevice(
	"Unknown SPI flash chip",

	0, 0, 0, 0, 0, 0, 0
);


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
			
			auto txData = std::make_unique<uint8_t[]>(txDataSize);

			txData[0] = PROTO_SYNC_BYTE;
			txData[1] = cmd;
			txData[2] = dataSize >> 8;
			txData[3] = dataSize;

			if (dataSize) {
				memcpy(txData.get() + 4, data, dataSize);
			}

			txData[txDataSize - 1] = crc8_get(txData.get(), txDataSize - 1, PROTO_CRC8_POLY, PROTO_CRC8_START);

//			debug_dumpBuffer(txData, txDataSize, 32, 0);

			ret = serial->write(serial, txData.get(), txDataSize, TIMEOUT_MS);
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
		auto buff = std::make_unique< uint8_t[]>(buffSize);

		buff[0] = txSize >> 8;
		buff[1] = txSize;
		buff[2] = rxSize >> 8;
		buff[3] = rxSize;

		memcpy(&buff[4], tx, txSize);

		ret = _cmdExecute(PROTO_CMD_SPI_TRANSFER, buff.get(), buffSize, rx, rxSize, rxWritten);
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
				(address >> 16) & 0xff,
				(address >>  8) & 0xff,
				(address >>  0) & 0xff,
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
			boost::this_thread::sleep_for(boost::chrono::milliseconds(timeIntervalMs));
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
			auto tx = std::make_unique<uint8_t[]>(4 + bufferSize);

			tx[0] = 0x02; // PP
			tx[1] = (address >> 16) & 0xff;
			tx[2] = (address >>  8) & 0xff;
			tx[3] = (address >>  0) & 0xff;

			memcpy(tx.get() + 4, buffer, bufferSize);

			ret = _spiTransfer(tx.get(), 4 + bufferSize, NULL, 0, NULL);
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
					(address >> 16) & 0xff, // Address
					(address >>  8) & 0xff,
					(address >>  0) & 0xff
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


typedef struct _Parameters {
	std::string serialPort;
	
	FILE *inputFd;
	FILE *outputFd;

	int idx;

	bool read;
	bool readBlock;
	bool readSector;

	bool erase;
	bool write;
	bool verify;

	bool unprotect;
} Parameters;

#if 0

	{ "unprotect",      no_argument,       0, 'u' },

	{ "flash-geometry", required_argument, 0, 'g' },

	{ 0, 0, 0, 0 }
};

static const char *shortOpts = "vhp:o:i:Rr:S:EWVug:";


static void _usage(const char *progName) {
	PRINTF(("\nUsage: %s", progName));

	struct option *optIt = longOpts;

	do {
		PRINTF(("  -%c --%s%s", optIt->val, optIt->name, optIt->has_arg ? " <arg>" : ""));

		optIt++;
	} while (optIt->name != NULL);

	PRINTF(("\nWhere:"));
	PRINTF(("  flash-geometry <block_size>:<block_count>:<sector_size>:<sector_count>:<unprotect-mask-hex>"));
	PRINTF(("    example: 65536:4:4096:64:8c"));
	PRINTF(("  input  - can be path to a regular file or '-' for stdin"));
	PRINTF(("  output - can be path to a regular file or '-' for stdout"));

	exit(1);
}
#endif

namespace po = boost::program_options;

#define OPT_VERBOSE "verbose"
#define OPT_HELP    "help"
#define OPT_SERIAL  "serial"
#define OPT_OUTPUT  "output"
#define OPT_INPUT   "input"

#define OPT_READ       "read"
#define OPT_READ_BLOCK "read-block"
#define OPT_READ_SECT  "read-sector"

#define OPT_ERASE     "erase"
#define OPT_WRITE     "write"
#define OPT_VERIFY    "verify"
#define OPT_UNPROTECT "unprotect"

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
				(OPT_VERBOSE    ",v",                           "Verbose output")
				(OPT_HELP       ",h",                           "Print usage message")
				(OPT_SERIAL     ",p", po::value<std::string>(), "Serial port path")
				(OPT_OUTPUT     ",o", po::value<std::string>(), "Output file path or '-' for stdout")
				(OPT_INPUT      ",i", po::value<std::string>(), "Input file path or '-' for stdin")
				(OPT_READ       ",R",                           "Read to output file")
				(OPT_READ_BLOCK ",r", po::value<off_t>(),       "Block index")
				(OPT_READ_SECT  ",S", po::value<off_t>(),       "Sector index")
				(OPT_ERASE      ",E",                           "Erase whole chip")
				(OPT_WRITE      ",W",                           "Write input file")
				(OPT_VERIFY     ",V",                           "Verify writing process")
				(OPT_UNPROTECT  ",u",                           "Unprotect the chip before doing any operation on it")
				(OPT_FLASH_DESC ",g",                           "Custom chip geometry in format <block_size>:<block_count>:<sector_size>:<sector_count>:<unprotect-mask-hex> (example: 65536:4:4096:64:8c)")
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

			if (vm.count(OPT_OUTPUT)) {
				auto output = vm[OPT_OUTPUT].as<std::string>();
				if (output == "-") {
					params.outputFd = stdout;

				} else {
					params.outputFd = fopen(output.c_str(), "w");
					if (params.outputFd == NULL) {
						PRINTF(("Unable to open output file! (%s)", output.c_str()));

						ret = 1;
						break;
					}
				}
			}

			if (vm.count(OPT_INPUT)) {
				auto input = vm[OPT_INPUT].as<std::string>();
				if (input == "-") {
					params.inputFd = stdin;

				}
				else {
					params.inputFd = fopen(input.c_str(), "w");
					if (params.inputFd == NULL) {
						PRINTF(("Unable to open input file! (%s)", input.c_str()));

						ret = 1;
						break;
					}
				}
			}

			if (vm.count(OPT_READ)) {
				params.read = true;
			}

			if (vm.count(OPT_READ_BLOCK)) {
				params.readBlock = vm[OPT_READ_BLOCK].as<off_t>();
			}

			if (vm.count(OPT_READ_SECT)) {
				params.readSector = vm[OPT_READ_SECT].as<off_t>();
			}

			if (vm.count(OPT_ERASE)) {
				params.erase = true;
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

		serial = new_serial(params.serialPort.c_str());
		if (serial == NULL) {
			break;
		}

		{
			const SpiFlashDevice *dev = NULL;

			SpiFlashInfo info;
			SpiFlashStatus status;

			if (! _spiFlashGetInfo(&info)) {
				break;
			}

			if (
				((info.deviceId[0] == 0xff) && (info.deviceId[0] == 0xff) && (info.manufacturerId == 0xff)) ||
				((info.deviceId[0] == 0x00) && (info.deviceId[0] == 0x00) && (info.manufacturerId == 0x00))
			) {
				PRINTF(("No flash device detected!"));

				break;
			}

			for (auto &chip : flashDevices) {
				if (
					(chip.id[0] == info.manufacturerId) &&
					(chip.id[1] == info.deviceId[0]) &&
					(chip.id[2] == info.deviceId[1])
				) {
					dev = &chip;
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

			PRINTF(("Flash chip: %s (%02x, %02x, %02x), size: %zdkB, blocks: %zd of %zdkB, sectors: %zd of %zdkB",
				dev->name, dev->id[0], dev->id[1], dev->id[2],
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
						break;
					}

					if (! _spiFlashBlockErase(block * dev->blockSize)) {
						break;
					}

					if (! _spiFlashWriteWait(&status, 100)) {
						break;
					}
				}
			}

			if (params.write) {
				uint8_t pageBuffer[PAGE_SIZE];
				size_t  pageWritten;
				uint32_t address = 0;

				if (params.inputFd == NULL) {
					PRINTF(("Writing requested but input file was not defined!"));

					ret = 1;
					break;
				}

				do {
					pageWritten = fread(pageBuffer, 1, sizeof(pageBuffer), params.inputFd);
					if (pageWritten > 0) {
						if (! _spiFlashWriteEnable()) {
							break;
						}

						if (! _spiFlashPageWrite(address, pageBuffer, pageWritten)) {
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
				if (params.outputFd == NULL) {
					PRINTF(("Reading requested but output file was not defined!"));

					ret = 1;
					break;
				}
			}

			if (params.read) {
				size_t   flashImageSize    = dev->blockSize * dev->blockCount;
				size_t   flashImageWritten = 0;
				
				if (flashImageSize == 0) {
					PRINTF(("Flash size is unknown!"));
					break;
				}

				PRINTF(("Reading whole flash"));

				auto flashImage = std::make_unique<uint8_t[]>(flashImageSize);

				if (_spiFlashRead(0, flashImage.get(), flashImageSize, &flashImageWritten)) {
					fwrite(flashImage.get(), 1, flashImageWritten, params.outputFd);
				}

			} else if (params.readBlock) {
				auto    buffer = std::make_unique< uint8_t[]>(dev->blockSize);
				size_t  bufferWritten;

				PRINTF(("Reading block %u (%#lx)", params.idx, params.idx * dev->blockSize));

				if (! _spiFlashRead(params.idx * dev->blockSize, buffer.get(), dev->blockSize, &bufferWritten)) {
					break;
				}

				fwrite(buffer.get(), 1, bufferWritten, params.outputFd);

			} else if (params.readSector) {
				auto    buffer = std::make_unique<uint8_t[]>(dev->sectorSize);
				size_t  bufferWritten;

				PRINTF(("Reading sector %u (%#lx)", params.idx, params.idx * dev->sectorSize));

				if (! _spiFlashRead(params.idx * dev->sectorSize, buffer.get(), dev->sectorSize, &bufferWritten)) {
					break;
				}

				fwrite(buffer.get(), 1, bufferWritten, params.outputFd);
			}

			if (params.verify) {
				auto referenceBuffer = std::make_unique<uint8_t[]>(dev->blockSize);
				auto buffer = std::make_unique<uint8_t[]>(dev->blockSize);
				size_t  bufferWritten;
				int     blockNo = 0;

				if (params.write) {
					fseek(params.inputFd, 0, SEEK_SET);

				} else {
					memset(referenceBuffer.get(), 0xff, dev->blockSize);
				}

				while (blockNo < dev->blockCount) {
					PRINTF(("Verifying block %d", blockNo));

					if (params.write) {
						fread(referenceBuffer.get(), 1, dev->blockSize, params.inputFd);
					}

					if (! _spiFlashRead(blockNo * dev->blockSize, buffer.get(), dev->blockSize, &bufferWritten)) {
						ret = 1;
						break;
					}

					if (memcmp(buffer.get(), referenceBuffer.get(), dev->blockSize) == 0) {
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

	free(serial);

	return ret;
}
