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

			debug_dumpBuffer(txData, txDataSize, 32, 0);

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


static bool _spiFlashBlockErase(uint32_t address) {
	bool ret = true;

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


#define SECTOR_SIZE 256


bool _spiFlashRead(uint32_t address, uint8_t *buffer, size_t bufferSize, size_t *bufferWritten) {
	bool ret = true;

	*bufferWritten = 0;

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

			uint8_t rx[SECTOR_SIZE];
			size_t rxWritten;

			while (bufferSize > 0) {
				uint16_t toRead = bufferSize >= SECTOR_SIZE ? SECTOR_SIZE : SECTOR_SIZE - bufferSize;

				DBG(("Reading %u bytes", toRead));

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

	FILE *inputFd;
	FILE *outputFd;

	bool erase;
	bool eraseBlock;
	int  eraseBlockIdx;
	bool eraseSector;
	int  eraseSectorIdx;

	bool read;
	bool readBlock;
	int  readBlockIdx;
	bool readSector;
	int  readSectorIdx;

	bool write;
	bool writeBlock;
	int  writeBlockIdx;
	bool writeSector;
	int  writeSectorIdx;

	bool unprotect;
} Parameters;


static struct option longOpts[] = {
	{ "verbose",      no_argument,       0, 'v' },
	{ "help",         no_argument,       0, 'h' },
	{ "serial",       required_argument, 0, 'p' },
	{ "output",       required_argument, 0, 'o' },
	{ "input",        required_argument, 0, 'i' },

	{ "erase",        no_argument,       0, 'E' },
	{ "erase-block",  required_argument, 0, 'e' },
	{ "erase-sector", required_argument, 0, 's' },

	{ "read",         no_argument,       0, 'R' },
	{ "read-block",   required_argument, 0, 'r' },
	{ "read-sector",  required_argument, 0, 'S' },

	{ "write",        no_argument,       0, 'W' },
	{ "write-block",  required_argument, 0, 'w' },
	{ "write-sector", required_argument, 0, 'b' },

	{ "unprotect",    no_argument,       0, 'u' },

	{ 0, 0, 0, 0 }
};

static const char *shortOpts = "vhp:Ee:s:Rr:S:Ww:b:u";


static void _usage(const char *progName) {
	PRINTF(("\nUsage: %s", progName));

	struct option *optIt = longOpts;

	do {
		PRINTF(("  -%c --%s%s", optIt->val, optIt->name, optIt->has_arg ? " <arg>" : ""));

		optIt++;
	} while (optIt->name != NULL);

	exit(1);
}


int main(int argc, char *argv[]) {
	int ret = 0;

	do {
		Parameters params;

		memset(&params, 0, sizeof(params));

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

					case 's':
						params.eraseSector    = true;
						params.eraseSectorIdx = atoi(optarg);
						break;

					case 'e':
						params.eraseBlock    = true;
						params.eraseBlockIdx = atoi(optarg);
						break;

					case 'E':
						params.erase = true;
						break;

					case 'S':
						params.readSector    = true;
						params.readSectorIdx = atoi(optarg);
						break;

					case 'r':
						params.readBlock    = true;
						params.readBlockIdx = atoi(optarg);
						break;

					case 'R':
						params.read = true;
						break;

					case 'b':
						params.writeSector    = true;
						params.writeSectorIdx = atoi(optarg);
						break;

					case 'w':
						params.writeBlock    = true;
						params.writeBlockIdx = atoi(optarg);
						break;

					case 'W':
						params.write = true;
						break;

					case 'u':
						params.unprotect = true;
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
		}

		serial = new_serial(params.serialPort);
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
					break;
				}

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

			if (params.erase) {
				PRINTF(("NOT IMPLEMENTED"));
				break;

			} else {
				if (params.eraseBlock) {
					if (! _spiFlashBlockErase(params.eraseBlockIdx * dev->blockSize)) {
						break;
					}
				}

				if (params.eraseSector) {
					PRINTF(("NOT IMPLEMENTED"));
					break;
				}
			}

			if (params.read) {
				PRINTF(("NOT IMPLEMENTED"));
				break;

			} else if (params.readBlock) {
				uint8_t buffer[dev->blockSize];
				size_t  bufferWritten;

				if (! _spiFlashRead(params.readBlockIdx * dev->blockSize, buffer, dev->blockSize, &bufferWritten)) {
					break;
				}

				debug_dumpBuffer(buffer, dev->blockSize, 32, 0);

			} else if (params.readSector) {
				PRINTF(("NOT IMPLEMENTED"));
				break;
			}

#if 0
			{
				size_t  flashSize = dev->blockCount * dev->blockSize;
				uint8_t flashData[flashSize];
				size_t  flashWritten;

				memset(flashData, 0, flashSize);

				if (! _spiFlashRead(0, flashData, flashSize, &flashWritten)) {
					break;
				}

				debug_dumpBuffer(flashData, 8192, 32, 0);

				{
					FILE *bin = fopen("/tmp/data.bin", "w");
					if (bin != NULL) {
						fwrite(flashData, 1, flashSize, bin);
						fclose(bin);
					}
				}
			}
#endif

#if 0
			// Disable write protection
			{
				if (! _spiFlashGetStatus(&status)) {
					break;
				}

				if (status.srWriteDisable || status.bp0 || status.bp1) {
					DBG(("Disabling Write protection"));

					if (! _spiFlashWriteEnable()) {
						ERR(("Unable to write enable"));
						break;
					}

					status.srWriteDisable = false;
					status.bp0            = false;
					status.bp1            = false;

					if (! _spiFlashWriteStatusRegister(&status)) {
						ERR(("Unable to write status register!"));
						break;
					}

					if (! _spiFlashGetStatus(&status)) {
						break;
					}

					if (status.srWriteDisable || status.bp0 || status.bp1) {
						ERR(("Unable to unlock chip!"));
						break;
					}
				}
			}
#endif
#if 0
			// Erase chip
			{
				if (! _spiFlashGetStatus(&status)) {
					ERR(("Unable to get status"));
					break;
				}

				PRINTF(("Status before erase : BP0: %u, BP1: %u, SRWD: %u, WEL: %u, WIP: %u",
					status.bp0, status.bp1, status.srWriteDisable, status.writeEnableLatch, status.writeInProgress
				));

				if (! _spiFlashWriteEnable()) {
					ERR(("Unable to write enable"));
					break;
				}

				if (! _spiFlashChipErase()) {
					ERR(("Unable to erase chip!"));
					break;
				}

				do {
					DBG(("Waiting on write finish"));

					if (! _spiFlashGetStatus(&status)) {
						ERR(("Unable to get status"));
						break;
					}

					sleep(1);
				} while (status.writeInProgress);
			}
#endif
#if 0
			{
				size_t  flashSize = 8192;//dev->blockCount * dev->blockSize;
				uint8_t flashData[flashSize];
				size_t  flashWritten;

				memset(flashData, 0, flashSize);

				if (! _spiFlashRead(0, flashData, flashSize, &flashWritten)) {
					break;
				}

				debug_dumpBuffer(flashData, 8192, 32, 0);

				{
					FILE *bin = fopen("/tmp/data.bin", "w");
					if (bin != NULL) {
						fwrite(flashData, 1, flashSize, bin);
						fclose(bin);
					}
				}
			}
#endif
		}
	} while (0);

	free(serial);

	return ret;
}
