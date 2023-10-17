#ifndef FLASHUTIL_DUMMYSPI_H_
#define FLASHUTIL_DUMMYSPI_H_

#include "flashutil/spi.h"
#include "flashutil/flash.h"


class DummySpi : public Spi {
	public:
		DummySpi(const Flash &flashInfo, size_t transferSize) : _flashInfo(flashInfo) {
			this->_capabilities.setTransferSizeMax(transferSize);
		}

		void transfer(Messages &msgs) override {

		}

		void chipSelect(bool select) override {

		}

		Config getConfig() override {
			return this->_config;
		}

		void setConfig(const Config &config) override {
			this->_config = config;
		}

		const Capabilities &getCapabilities() const override {
			return this->_capabilities;
		}

		void attach() override {
		}

		void detach() override {
		}

	private:
		Config       _config;
		Capabilities _capabilities;
		Flash        _flashInfo;
};

#endif /* FLASHUTIL_DUMMYSPI_H_ */
