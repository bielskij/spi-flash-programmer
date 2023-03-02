/*
 * main.c
 *
 *  Created on: 27 lut 2022
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>

#include "crc8.h"
#include "protocol.h"
#include "serial.h"
#include "debug.h"

#define TIMEOUT_MS 1000


typedef struct _SpiFlashDevice {
	char  *name;
	uint8_t id[5];
	char    idLen;
	size_t  blockSize;
	size_t  blockCount;
	size_t  sectorSize;
	size_t  sectorCount;
	size_t  pageSize;
	uint8_t protectMask;
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
	bool help;

	char *serialPort;
	char *outputFile;
	char *inputFile;

	int baud;

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


static struct option longOpts[] = {
	{ "verbose",        no_argument,       0, 'v' },
	{ "help",           no_argument,       0, 'h' },
	{ "serial",         required_argument, 0, 'p' },
	{ "output",         required_argument, 0, 'o' },
	{ "input",          required_argument, 0, 'i' },
	{ "baud",           required_argument, 0, 'b' },

	{ "read",           no_argument,       0, 'R' },
	{ "read-block",     required_argument, 0, 'r' },
	{ "read-sector",    required_argument, 0, 'S' },

	{ "erase",          no_argument,       0, 'E' },
	{ "write",          no_argument,       0, 'W' },
	{ "verify",         no_argument,       0, 'V' },

	{ "unprotect",      no_argument,       0, 'u' },

	{ "flash-geometry", required_argument, 0, 'g' },

	{ 0, 0, 0, 0 }
};

static const char *shortOpts = "vhp:o:i:b:Rr:S:EWVug:";


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


int main(int argc, char *argv[]) {
	int ret = 0;

	do {
		Parameters params;

		memset(&params, 0, sizeof(params));

		params.baud = 1000000;

		{
			int option;
			int longIndex;

			while ((option = getopt_long(argc, argv, shortOpts, longOpts, &longIndex)) != -1) {
				switch (option) {
					case 'h':
						params.help = true;
						break;

					case 'p':
						params.serialPort = strdup(optarg);
						break;

					case 'o':
						params.outputFile = strdup(optarg);
						break;

					case 'i':
						params.inputFile = strdup(optarg);
						break;

					case 'b':
						params.baud = atoi(optarg);
						break;

					case 'E':
						params.erase = true;
						break;

					case 'S':
						params.readSector = true;
						params.idx        = atoi(optarg);
						break;

					case 'r':
						params.readBlock = true;
						params.idx       = atoi(optarg);
						break;

					case 'R':
						params.read = true;
						break;

					case 'W':
						params.write = true;
						break;

					case 'V':
						params.verify = true;
						break;

					case 'u':
						params.unprotect = true;
						break;

					case 'g':
						{
							int blockCount;
							int blockSize;
							int sectorCount;
							int sectorSize;
							uint8_t unprotectMask;

							if (sscanf(
								optarg, "%d:%d:%d:%d:%02hhx",
								&blockSize, &blockCount,
								&sectorSize, &sectorCount,
								&unprotectMask
							) != 5
							) {
								PRINTF(("Invalid flash-geometry syntax!"));

								_usage(argv[0]);
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
						break;

					default:
						params.help = true;
				}
			}

			if (params.help) {
				_usage(argv[0]);
			}

			if (params.serialPort == NULL) {
				PRINTF(("Serial port was not provided!"));

				_usage(argv[0]);
			}

			if (params.outputFile != NULL) {
				if (strcmp(params.outputFile, "-") == 0) {
					params.outputFd = stdout;

				} else {
					params.outputFd = fopen(params.outputFile, "w");
					if (params.outputFd == NULL) {
						PRINTF(("Unable to open output file! (%s)", params.outputFile));

						ret = 1;
						break;
					}
				}
			}

			if (params.inputFile != NULL) {
				if (strcmp(params.inputFile, "-") == 0) {
					params.inputFd = stdin;

				} else {
					params.inputFd = fopen(params.inputFile, "r");
					if (params.inputFd == NULL) {
						PRINTF(("Unable to open input file! (%s)", params.inputFile));

						ret = 1;
						break;
					}
				}
			}
		}

		serial = new_serial(params.serialPort, params.baud);
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
				uint8_t *flashImage        = NULL;

				if (flashImageSize == 0) {
					PRINTF(("Flash size is unknown!"));
					break;
				}

				PRINTF(("Reading whole flash"));

				flashImage = malloc(flashImageSize);

				if (_spiFlashRead(0, flashImage, flashImageSize, &flashImageWritten)) {
					fwrite(flashImage, 1, flashImageWritten, params.outputFd);
				}

				free(flashImage);

			} else if (params.readBlock) {
				uint8_t buffer[dev->blockSize];
				size_t  bufferWritten;

				PRINTF(("Reading block %u (%#lx)", params.idx, params.idx * dev->blockSize));

				if (! _spiFlashRead(params.idx * dev->blockSize, buffer, dev->blockSize, &bufferWritten)) {
					break;
				}

				fwrite(buffer, 1, bufferWritten, params.outputFd);

			} else if (params.readSector) {
				uint8_t buffer[dev->sectorSize];
				size_t  bufferWritten;

				PRINTF(("Reading sector %u (%#lx)", params.idx, params.idx * dev->sectorSize));

				if (! _spiFlashRead(params.idx * dev->sectorSize, buffer, dev->sectorSize, &bufferWritten)) {
					break;
				}

				fwrite(buffer, 1, bufferWritten, params.outputFd);
			}

			if (params.verify) {
				uint8_t referenceBuffer[dev->blockSize];
				uint8_t buffer[dev->blockSize];
				size_t  bufferWritten;
				int     blockNo = 0;

				if (params.write) {
					fseek(params.inputFd, 0, SEEK_SET);

				} else {
					memset(referenceBuffer, 0xff, dev->blockSize);
				}

				while (blockNo < dev->blockCount) {
					PRINTF(("Verifying block %d", blockNo));

					if (params.write) {
						fread(referenceBuffer, 1, dev->blockSize, params.inputFd);
					}

					if (! _spiFlashRead(blockNo * dev->blockSize, buffer, dev->blockSize, &bufferWritten)) {
						ret = 1;
						break;
					}

					if (memcmp(buffer, referenceBuffer, dev->blockSize) == 0) {
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
