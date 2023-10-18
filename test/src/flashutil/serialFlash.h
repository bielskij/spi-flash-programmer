#ifndef FLASHUTIL_DUMMYSPI_H_
#define FLASHUTIL_DUMMYSPI_H_

#include "flashutil/spi.h"
#include "flashutil/spi/serial.h"
#include "flashutil/flash.h"


class SerialFlash : public Serial {
	public:
		SerialFlash(const Flash &flashInfo, size_t transferSize);
		virtual ~SerialFlash();

		virtual void write(void *buffer, std::size_t bufferSize, int timeoutMs) override;
		virtual void read(void *buffer, std::size_t bufferSize, int timeoutMs) override;

		const Flash &getFlashInfo();

	private:
		class Impl;

		std::unique_ptr<Impl> _self;
};

#endif /* FLASHUTIL_DUMMYSPI_H_ */
