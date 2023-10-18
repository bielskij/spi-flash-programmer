#include "flashutil/spi/creator.h"

#include "impl/serial.h"


SpiCreator::SpiCreator() {

}


SpiCreator::~SpiCreator() {

}


std::unique_ptr<Spi> SpiCreator::createSerialSpi(const std::string &path, int baudrate) {
	return std::unique_ptr<Spi>(new SerialSpi(path, baudrate));
}


std::unique_ptr<Spi> SpiCreator::createSerialSpi(Serial &serial) {
	return std::unique_ptr<Spi>(new SerialSpi(serial));
}
