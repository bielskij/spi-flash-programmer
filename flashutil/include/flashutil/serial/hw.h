/*
 * serial.h
 *
 *  Created on: 6 mar 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef FLASHUTIL_SERIAL_HW_H_
#define FLASHUTIL_SERIAL_HW_H_

#include <memory>
#include <string>

#include "flashutil/serial.h"

class HwSerial : public Serial {
	public:
		HwSerial(const std::string &serialPath, int baud);
		~HwSerial();

	public:
		void write(void *buffer, std::size_t bufferSize, int timeoutMs) override;
		void read(void *buffer, std::size_t bufferSize, int timeoutMs) override;

	private:
		void _flush();

	private:
		class Impl;

		std::unique_ptr<Impl> self;
};

#endif /* FLASHUTIL_SERIAL_HW_H_ */
