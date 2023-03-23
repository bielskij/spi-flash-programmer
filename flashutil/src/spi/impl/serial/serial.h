/*
 * serial.h
 *
 *  Created on: 6 mar 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef SPI_IMPL_SERIAL_SERIAL_H_
#define SPI_IMPL_SERIAL_SERIAL_H_

#include <memory>
#include <string>

class Serial {
	public:
		Serial(const std::string &serialPath, int baud);
		virtual ~Serial();

	public:
		void write(void *buffer, std::size_t bufferSize, int timeoutMs);
		void read(void *buffer, std::size_t bufferSize, int timeoutMs);

	private:
		void _flush();

	private:
		class Impl;

		std::unique_ptr<Impl> self;
};

#endif /* SPI_IMPL_SERIAL_SERIAL_H_ */
