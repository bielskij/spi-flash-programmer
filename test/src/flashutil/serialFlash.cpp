#include "serialFlash.h"

#define DEBUG 1
#include "flashutil/debug.h"


struct SerialFlash::Impl {
	Spi::Config       config;
	Spi::Capabilities capabilities;
	Flash             flashInfo;

	Impl(const Flash &flashInfo, size_t transferSize) : flashInfo(flashInfo) {
		this->capabilities.setTransferSizeMax(transferSize);
	}

	const Flash &getFlashInfo() {
		return this->flashInfo;
	}

	void write(void *buffer, std::size_t bufferSize, int timeoutMs) {
		DBG(("CALL"));
	}

	void read(void *buffer, std::size_t bufferSize, int timeoutMs) {
		DBG(("CALL"));
	}
};


SerialFlash::SerialFlash(const Flash &flashInfo, size_t transferSize) {
	this->_self = std::make_unique<Impl>(flashInfo, transferSize);
}


SerialFlash::~SerialFlash() {
}


void SerialFlash::write(void *buffer, std::size_t bufferSize, int timeoutMs) {
	this->_self->write(buffer, bufferSize, timeoutMs);
}


void SerialFlash::read(void *buffer, std::size_t bufferSize, int timeoutMs) {
	this->_self->read(buffer, bufferSize, timeoutMs);
}


const Flash &SerialFlash::getFlashInfo() {
	return this->_self->getFlashInfo();
}
