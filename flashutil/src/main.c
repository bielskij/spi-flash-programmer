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
#include <string.h>
#include <ctype.h>

#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>


#include "protocol.h"


#define ERR(x) { printf("[ERR %s:%d]: ", __FUNCTION__, __LINE__); printf x; printf("\r\n"); }
#define DBG(x) { printf("[DBG %s:%d]: ", __FUNCTION__, __LINE__); printf x; printf("\r\n"); }
#define PRINTF(x) { printf x; printf("\n"); }

#define TIMEOUT_MS 1000

static int _fd = -1;


typedef struct _SpiFlashDevice {
	char  *name;
	uint8_t id[5];
	char    idLen;
	size_t  blockSize;
	size_t  blockCount;
	size_t  pageSize;
} SpiFlashDevice;


#define INFO(_jedec_id, _ext_id, _sector_size, _n_sectors)         \
	.id = {                                                        \
		((_jedec_id) >> 16) & 0xff,                                \
		((_jedec_id) >> 8) & 0xff,                                 \
		(_jedec_id) & 0xff,                                        \
		((_ext_id) >> 8) & 0xff,                                   \
		(_ext_id) & 0xff,                                          \
	},                                                             \
	.idLen      = (!(_jedec_id) ? 0 : (3 + ((_ext_id) ? 2 : 0))), \
	.blockSize  = (_sector_size),                                 \
	.blockCount = (_n_sectors),                                   \
	.pageSize   = 256,

static const SpiFlashDevice flashDevices[] = {
	{
		"Macronix MX25L2026E/MX25l2005A",

		INFO(0xc22012, 0, 64 * 1024, 4)
	}
};

static const int flashDevicesCount = sizeof(flashDevices) / sizeof(flashDevices[0]);


uint8_t crc8_getForByte(uint8_t byte, uint8_t polynomial, uint8_t start) {
	uint8_t remainder = start;

	remainder ^= byte;

	{
		uint8_t bit;

		for (bit = 0; bit < 8; bit++) {
			if (remainder & 0x01) {
				remainder = (remainder >> 1) ^ polynomial;

			} else {
				remainder = (remainder >> 1);
			}

		}
	}

	return remainder;
}


uint8_t crc8_get(uint8_t *buffer, uint16_t bufferSize, uint8_t polynomial, uint8_t start) {
	uint8_t remainder = start;
	uint16_t byte;

	// Perform modulo-2 division, a byte at a time.
	for (byte = 0; byte < bufferSize; ++byte) {
		remainder = crc8_getForByte(buffer[byte], polynomial, remainder);
	}

	return remainder;
}


void debug_dumpBuffer(uint8_t *buffer, uint32_t bufferSize, uint32_t lineLength, uint32_t offset) {
	uint32_t i;

	char asciiBuffer[lineLength + 1];

	for (i = 0; i < bufferSize; i++) {
		if (i % lineLength == 0) {
			if (i != 0) {
				printf("  %s\n", asciiBuffer);
			}

			printf("%04x:  ", i + offset);
		}

		printf(" %02x", buffer[i]);

		if (! isprint(buffer[i])) {
			asciiBuffer[i % lineLength] = '.';
		} else {
			asciiBuffer[i % lineLength] = buffer[i];
		}

		asciiBuffer[(i % lineLength) + 1] = '\0';
	}

	while ((i % 16) != 0) {
		printf("   ");
		i++;
	}

	printf("  %s\n", asciiBuffer);
}


bool _serialSelect(bool read, uint32_t _timeout) {
	bool ret = false;

	{
		fd_set set;
		struct timeval timeout;

		FD_ZERO(&set);
		FD_SET(_fd, &set);

		timeout.tv_sec  = _timeout / 1000;
		timeout.tv_usec = (_timeout % 1000) * 1000;

		int selectRet = select(_fd + 1, read ? &set : NULL, read ? NULL : &set, NULL, &timeout);
		if (selectRet > 0) {
			if (FD_ISSET(_fd, &set)) {
				ret = true;
			}
		}
	}

	return ret;
}


bool _serialGet(uint8_t *byte) {
	bool ret = false;

	do {
		if (
			! _serialSelect(true, TIMEOUT_MS) ||
			read(_fd, byte, 1) != 1
		) {
			ERR(("%s(): Error reading byte! (%m)", __func__));
			break;
		}

		ret = true;
	} while (0);

	return ret;
}


bool _serialWrite(uint8_t *buffer, uint32_t bufferSize) {
	bool ret = false;

	do {
		if (! _serialSelect(false, TIMEOUT_MS)) {
			break;
		}

		if (write(_fd, buffer, bufferSize) != bufferSize) {
			DBG(("%s(): Error writing data!", __func__));
			break;
		}

		ret = true;
	} while (0);

	return ret;
}


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

			ret = _serialWrite(txData, txDataSize);
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
				ret = _serialGet(&tmp);
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
				ret = _serialGet(&tmp);
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
					ret = _serialGet(&tmp);
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
					ret = _serialGet(&tmp);
					if (! ret) {
						break;
					}

					crc = crc8_getForByte(tmp, PROTO_CRC8_POLY, crc);

					if (i < responseSize) {
						if (response != NULL) {
							response[i] = tmp;
						}

						if (responseWritten != NULL) {
							*responseWritten = *responseWritten + 1;
						}
					}
				}

				if (! ret) {
					break;
				}
			}

			// CRC
			{
				ret = _serialGet(&tmp);
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
	bool bp1; // level of protected block
	bool bp0;

	bool writeEnableLatch;
	bool writeInProgress;

	bool srWriteDisable;
} SpiFlashStatus;


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

			status->srWriteDisable   = (rx[1] & 0x80) != 0;
			status->bp1              = (rx[1] & 0x08) != 0;
			status->bp0              = (rx[1] & 0x04) != 0;
			status->writeEnableLatch = (rx[1] & 0x02) != 0;
			status->writeInProgress  = (rx[1] & 0x01) != 0;
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
				0x06, // WREN
				0x01, // WRSR
				0x00
			};

			if (status->srWriteDisable) {
				tx[1] |= 0x80;
			}

			if (status->bp0) {
				tx[1] |= 0x04;
			}

			if (status->bp1) {
				tx[1] |= 0x08;
			}

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
				0x06, // WREN
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


int main(int argc, char *argv[]) {
	const char *ttyPath = argv[1];

	do {
		_fd = open(ttyPath, O_RDWR | O_NOCTTY | O_NONBLOCK);
		if (_fd < 0) {
			PRINTF(("Unable to open rfcomm device %s (%m)", argv[1]));
			break;
		}

		{
			struct termios options;
			int opRet = 0;

			memset(&options, 0, sizeof(options));

			opRet = tcgetattr(_fd, &options);
			if (opRet != 0) {
				break;
			}

			cfmakeraw(&options);
			cfsetospeed(&options, B38400);
			cfsetispeed(&options, B38400);

			options.c_cflag |= (CLOCAL | CREAD);
			options.c_cflag &= ~PARENB;
			options.c_cflag &= ~CSTOPB;
			options.c_cflag &= ~HUPCL; // Lower modem control lines after last process closes the device - prevent form arduino reset on next call
			options.c_cflag &= ~CSIZE;
			options.c_cflag |= CS8;
			options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
			options.c_oflag &= ~OPOST;

			options.c_cc[VMIN]  = 1;
			options.c_cc[VTIME] = 0;

			opRet = tcsetattr(_fd, TCSANOW, &options);
			if (opRet != 0) {
				break;
			}

			tcflush(_fd, TCIOFLUSH);
		}

		PRINTF(("Device %s opened!", ttyPath));

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

			PRINTF(("Have flash %02x, %02x, %02x of size: %u bytes",
				info.manufacturerId, info.deviceId[0], info.deviceId[1], 1 << info.deviceId[1]
			));

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
				break;
			}

			if (! _spiFlashGetStatus(&status)) {
				break;
			}

			PRINTF(("Status: BP0: %u, BP1: %u, SRWD: %u, WEL: %u, WIP: %u",
				status.bp0, status.bp1, status.srWriteDisable, status.writeEnableLatch, status.writeInProgress
			));

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
#if 1
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
#if 1
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

	return 0;
}
