/*
 * serial.h
 *
 *  Created on: 6 mar 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef FLASHUTIL_SERIAL_H_
#define FLASHUTIL_SERIAL_H_

#include <memory>
#include <string>

class Serial {
	public:
		virtual ~Serial() {
		}

	public:
		virtual void write(void *buffer, std::size_t bufferSize, int timeoutMs) = 0;
		virtual void read(void *buffer, std::size_t bufferSize, int timeoutMs)  = 0;
};

#endif /* FLASHUTIL_SERIAL_H_ */
