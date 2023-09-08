/*
 * serial.h
 *
 *  Created on: 6 mar 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef SPI_IMPL_SERIAL_H_
#define SPI_IMPL_SERIAL_H_

#include <string>
#include <memory>

#include "spi.h"

class SerialSpi : public Spi {
	public:
		SerialSpi(const std::string &path, int baudrate);
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

#endif /* SPI_IMPL_SERIAL_H_ */
