#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

#include "serial.h"
#include "debug.h"


typedef struct _SerialImpl {
	Serial base;

	int fd;
	struct termios origTermios;
	bool           origTermiosSet;
} SerialImpl;


bool _serialSelect(SerialImpl *self, bool read, int _timeout) {
	bool ret = false;

	{
		fd_set set;
		struct timeval timeout;

		FD_ZERO(&set);
		FD_SET(self->fd, &set);

		timeout.tv_sec  = _timeout / 1000;
		timeout.tv_usec = (_timeout % 1000) * 1000;

		int selectRet = select(self->fd + 1, read ? &set : NULL, read ? NULL : &set, NULL, &timeout);
		if (selectRet > 0) {
			if (FD_ISSET(self->fd, &set)) {
				ret = true;
			}
		}
	}

	return ret;
}


static bool _write(struct _Serial *self, void *buffer, size_t bufferSize, int timeoutMs) {
	bool ret = false;

	do {
		SerialImpl *s = (SerialImpl *) self;

		if (! _serialSelect(s, false, timeoutMs)) {
			break;
		}

		if (write(s->fd, buffer, bufferSize) != bufferSize) {
			DBG(("%s(): Error writing data!", __func__));
			break;
		}

		ret = true;
	} while (0);

	return ret;
}


static bool _readByte(struct _Serial *self, uint8_t *value, int timeoutMs) {
	bool ret = false;

	do {
		SerialImpl *s = (SerialImpl *) self;

		if (
			! _serialSelect(s, true, timeoutMs) ||
			read(s->fd, value, 1) != 1
		) {
			ERR(("%s(): Error reading byte! (%m)", __func__));
			break;
		}

		ret = true;
	} while (0);

	return ret;
}


Serial *new_serial(const char *serialPath, int baud) {
	SerialImpl *ret = malloc(sizeof(*ret));

	do {
		memset(ret, 0, sizeof(*ret));

		ret->fd = open(serialPath, O_RDWR | O_NOCTTY | O_NONBLOCK);
		if (ret->fd < 0) {
			PRINTF(("Unable to open serial device %s (%m)", serialPath));
			break;
		}

		{
			int opRet;

			struct termios options;

			opRet = tcgetattr(ret->fd, &ret->origTermios);
			if (opRet != 0) {
				PRINTF(("Error getting serial attributes!"));
				break;
			}

			ret->origTermiosSet = true;

			memcpy(&options, &ret->origTermios, sizeof(&options));

			{
				speed_t speedbaud = B0;

				switch (baud) {
					case 1000000: speedbaud = B1000000; break;
					case  500000: speedbaud =  B500000; break;
					case   38400: speedbaud =   B38400; break;
					case   19200: speedbaud =   B19200; break;
					default:
						break;
				}

				if (speedbaud == B0) {
					PRINTF(("Not supported baud rate! (%d)", baud));
					break;
				}

				if (
					(cfsetospeed(&options, speedbaud) != 0) ||
					(cfsetispeed(&options, speedbaud) != 0)
				) {
					PRINTF(("Unable to set in/out speed to %d bps", baud));
					break;
				}
			}

			// 8bit
			options.c_cflag = (options.c_cflag & ~CSIZE) | CS8;

			// Set into raw, no echo mode
			options.c_iflag  = IGNBRK;
			options.c_lflag  = 0;
			options.c_oflag  = 0;
			options.c_cflag |= (CLOCAL | CREAD);
			// disable in/out control
			options.c_iflag &= ~(IXON | IXOFF | IXANY);
			// No parity
			options.c_cflag &= ~(PARENB | PARODD);
			// 1bit stop
			options.c_cflag &= ~CSTOPB;
			options.c_cc[VMIN]  = 1;
			options.c_cc[VTIME] = 5;

			opRet = tcsetattr(ret->fd, TCSANOW, &options);
			if (opRet != 0) {
				PRINTF(("Error setting serial attributes!"));
				break;
			}

			// set RTS
			{
				int mcs = 0;

				ioctl(ret->fd, TIOCMGET, &mcs);
				{
					mcs |= TIOCM_RTS;
				}
				ioctl(ret->fd, TIOCMSET, &mcs);
			}

			// Disable hw flow control
			{
				struct termios tty;

				tcgetattr(ret->fd, &tty);
				{
					tty.c_cflag &= ~CRTSCTS;
				}
				tcsetattr(ret->fd, TCSANOW, &tty);
			}

			// set line status
			{
				struct termios tty;

				tcgetattr(ret->fd, &tty);
				{
					tty.c_cflag |= CLOCAL;
				}
				tcsetattr(ret->fd, TCSANOW, &tty);
			}

			{
				bool doSleep = false;

				// disable hangup on close - prevent from Arduino reset
				{
					struct termios tty;

					tcgetattr(ret->fd, &tty);
					{
						if ((tty.c_cflag & HUPCL) != 0) {
							doSleep = true;

							tty.c_cflag &= ~HUPCL;
						}
					}
					tcsetattr(ret->fd, TCSANOW, &tty);
				}

				if (doSleep) {
					close(ret->fd);
				}
			}
		}

		ret->base.readByte = _readByte;
		ret->base.write    = _write;

		PRINTF(("Device %s opened and configured for speed %dbps!", serialPath, baud));

		return ret;
	} while (0);

	free_serial(ret);

	return NULL;
}


void free_serial(Serial *serial) {
	SerialImpl *s = (SerialImpl *) serial;


}
