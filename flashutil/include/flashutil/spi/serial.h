/*
 * serial.h
 *
 *  Created on: 6 mar 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef FLASHUTIL_SPI_SERIAL_H_
#define FLASHUTIL_SPI_SERIAL_H_

#include <string>
#include <memory>

#include "flashutil/spi.h"
#include "flashutil/serial.h"

class SerialSpi : public Spi {
	public:
		SerialSpi(Serial &serial);
		~SerialSpi();

		void transfer(Messages &msgs) override;
		void chipSelect(bool select) override;

		Config getConfig() override;
		void   setConfig(const Config &config) override;

		const Capabilities &getCapabilities() const override;
		void attach() override;
		void detach() override;

	private:
		class Impl;

		std::unique_ptr<Impl> self;
};

#endif /* FLASHUTIL_SPI_SERIAL_H_ */
