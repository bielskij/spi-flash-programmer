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
			uint8_t  tx[32];
			uint16_t txSize = 0;

			tx[txSize++] = PROTO_SYNC_BYTE;
			tx[txSize++] = PROTO_CMD_SPI_TRANSFER;
			tx[txSize++] = 0;
			tx[txSize++] = 4;

			tx[txSize++] = 0;
			tx[txSize++] = 0;
			tx[txSize++] = 0;
			tx[txSize++] = 8;

			tx[txSize++] = crc8_get(tx, txSize, PROTO_CRC8_POLY, PROTO_CRC8_START);

			_serialWrite(tx, txSize);

			uint8_t rx[64];
			uint16_t rxSize = 0;

			while (_serialGet(&rx[rxSize])) {
				rxSize++;
			}

			DBG(("Read %d bytes %02x == %02x", rxSize, rx[rxSize - 1], crc8_get(rx, rxSize - 1, PROTO_CRC8_POLY, PROTO_CRC8_START)));

			debug_dumpBuffer(rx, rxSize, 32, 0);
		}
	} while (0);

	return 0;
}
