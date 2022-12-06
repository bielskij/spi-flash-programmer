#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

#include "serial.h"
#include "debug.h"


typedef struct _SerialImpl {
	Serial base;

	int fd;
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
	SerialImpl *ret = NULL;

	int fd = -1;

	do {
		fd = open(serialPath, O_RDWR | O_NOCTTY | O_NONBLOCK);
		if (fd < 0) {
			PRINTF(("Unable to open serial device %s (%m)", serialPath));
			break;
		}

		{
			struct termios options;
			int opRet = 0;

			memset(&options, 0, sizeof(options));

			opRet = tcgetattr(fd, &options);
			if (opRet != 0) {
				PRINTF(("Error getting serial attributes!"));
				break;
			}

			cfmakeraw(&options);

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

			opRet = tcsetattr(fd, TCSANOW, &options);
			if (opRet != 0) {
				PRINTF(("Error setting serial attributes!"));
				break;
			}

			tcflush(fd, TCIOFLUSH);
		}

		ret = malloc(sizeof(*ret));
		ret->fd = fd;
		ret->base.readByte = _readByte;
		ret->base.write    = _write;

		PRINTF(("Device %s opened and configured for speed %dbps!", serialPath, baud));
	} while (0);

	if (ret == NULL) {
		if (fd >= 0) {
			close(fd);
		}
	}

	return (Serial *) ret;
}
