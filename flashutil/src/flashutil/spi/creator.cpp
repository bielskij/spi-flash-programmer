#include "flashutil/spi/creator.h"

#include "impl/serial.h"


SpiCreator::SpiCreator() {

}


SpiCreator::~SpiCreator() {

}


std::unique_ptr<Spi> SpiCreator::createSerialSpi(const std::string &path, int baudrate) {
	return std::unique_ptr<Spi>(new SerialSpi(path, baudrate));
}
