#ifndef FLASHUTIL_DUMMYSPI_H_
#define FLASHUTIL_DUMMYSPI_H_

#include "flashutil/spi.h"
#include "flashutil/spi/serial.h"
#include "flashutil/flash.h"


class SerialProgrammer : public Serial {
	public:
		SerialProgrammer(const Flash &flashInfo, size_t transferSize);
		virtual ~SerialProgrammer();

		virtual void write(void *buffer, std::size_t bufferSize, int timeoutMs) override;
		virtual void read(void *buffer, std::size_t bufferSize, int timeoutMs) override;

	private:
		class Impl;

		std::unique_ptr<Impl> _self;
};

#endif /* FLASHUTIL_DUMMYSPI_H_ */
