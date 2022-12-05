/*
 * SerialSpi.cpp
 *
 *  Created on: 5 gru 2022
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#include <cstdlib>

#if defined(__unix__)
#include <fcntl.h>
#include <termios.h>
#endif

#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include "serial.h"


struct flashutil::SerialSpi::Impl {
	bool readByte(uint8_t byte, uint32_t timeoutMs) {
		return false;
	}

	bool write(uint8_t *buffer, size_t bufferSize, uint32_t timeoutMs) {
		return false;
	}
};


flashutil::SerialSpi::SerialSpi(const std::string &serial) {

}


flashutil::SerialSpi::~SerialSpi() {
}


bool flashutil::SerialSpi::transfer(const Buffer &tx, Buffer *rx) {

}
