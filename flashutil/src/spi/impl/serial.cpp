#include <cstring>

#include "crc8.h"
#include "protocol.h"

#include "serial.h"
#include "serial/serial.h"

//#define DEBUG 1
#include "debug.h"


#define TIMEOUT_MS 1000


struct SerialSpi::Impl {
	std::unique_ptr<Serial> serial;

	Impl(const std::string &path, int baudrate) {
		this->serial.reset(new Serial(path, baudrate));
	}
};


SerialSpi::SerialSpi(const std::string &path, int baudrate) {
	this->self.reset(new SerialSpi::Impl(path, baudrate));
}


SerialSpi::~SerialSpi() {

}


void SerialSpi::chipSelect(bool select) {
	this->spiCs(! select);
}


void SerialSpi::transfer(Messages &msgs) {
	for (size_t i = 0; i < msgs.count(); i++) {
		auto &msg = msgs.at(i);

		const size_t rxSize = msg.recv().getBytes() + msg.recv().getSkips();
		const size_t txSize = msg.send().getBytes();

		size_t rxWritten;

		uint8_t txBuffer[txSize];
		uint8_t rxBuffer[rxSize];

		memcpy(txBuffer, msg.send().data(), txSize);

		if (msg.isAutoChipSelect()) {
			this->spiCs(false);
		}

		if (this->spiTransfer(txBuffer, txSize, rxBuffer, rxSize, &rxWritten)) {
			std::size_t dstOffset = 0;

			auto skipIndex = msg.recv().getSkipMap();

			for (int i = 0; i < rxWritten; i++) {
				if (skipIndex.find(i) != skipIndex.end()) {
					continue;
				}

				msg.recv().data()[dstOffset++] = rxBuffer[i];
			}
		}

		if (msg.isAutoChipSelect()) {
			this->spiCs(true);
		}
	}
}


bool SerialSpi::cmdExecute(uint8_t cmd, uint8_t *data, size_t dataSize, uint8_t *response, size_t responseSize, size_t *responseWritten) {
	bool ret = true;

	do {
		DBG(("CALL cmd: %d (%02x), %p, %zd, %p, %zd, %p", cmd, cmd, data, dataSize, response, responseSize, responseWritten));

		if (responseWritten != NULL) {
			*responseWritten = 0;
		}

		// Send
		{
			uint16_t txDataSize = 4 + dataSize + 1;
			uint8_t txData[txDataSize];

			txData[0] = PROTO_SYNC_BYTE;
			txData[1] = cmd;
			txData[2] = dataSize >> 8;
			txData[3] = dataSize;

			if (dataSize) {
				memcpy(txData + 4, data, dataSize);
			}

			txData[txDataSize - 1] = crc8_get(txData, txDataSize - 1, PROTO_CRC8_POLY, PROTO_CRC8_START);

//			debug_dumpBuffer(txData, txDataSize, 32, 0);

			self->serial->write(txData, txDataSize, TIMEOUT_MS);
		}

		// Receive
		{
			uint8_t crc = PROTO_CRC8_START;
			uint8_t tmp;

			// sync
			{
				tmp = self->serial->readByte(TIMEOUT_MS);

				ret = tmp == PROTO_SYNC_BYTE;
				if (! ret) {
					ERR(("Received byte is not a sync byte!"));
					break;
				}

				crc = crc8_getForByte(tmp, PROTO_CRC8_POLY, crc);
			}

			// code
			{
				tmp = self->serial->readByte(TIMEOUT_MS);

				if (tmp != PROTO_NO_ERROR) {
					ERR(("Received error! (%#02x)", tmp));
				}

				crc = crc8_getForByte(tmp, PROTO_CRC8_POLY, crc);
			}

			// length
			{
				uint16_t dataLen = 0;

				for (int i = 0; i < 2; i++) {
					tmp = self->serial->readByte(TIMEOUT_MS);

					dataLen |= (tmp << (8 * (1 - i)));

					crc = crc8_getForByte(tmp, PROTO_CRC8_POLY, crc);
				}

				if (! ret) {
					break;
				}

				// Data
				for (uint16_t i = 0; i < dataLen; i++) {
					tmp = self->serial->readByte(TIMEOUT_MS);

					crc = crc8_getForByte(tmp, PROTO_CRC8_POLY, crc);

					if (i < responseSize) {
						if (response != NULL) {
							response[i] = tmp;
						}

						if (responseWritten != NULL) {
							*responseWritten += 1;
						}
					}
				}

				if (! ret) {
					break;
				}
			}

			// CRC
			{
				tmp = self->serial->readByte(TIMEOUT_MS);

				ret = tmp == crc;
				if (! ret) {
					ERR(("CRC mismatch!"));
					break;
				}
			}
		}

//		debug_dumpBuffer(response, responseSize, 32, 0);
	} while (0);

	return ret;
}


bool SerialSpi::spiCs(bool high) {
	return this->cmdExecute(high ? PROTO_CMD_SPI_CS_HI : PROTO_CMD_SPI_CS_LO, NULL, 0, NULL, 0, NULL);
}


bool SerialSpi::spiTransfer(uint8_t *tx, uint16_t txSize, uint8_t *rx, uint16_t rxSize, size_t *rxWritten) {
	bool ret = true;

	do {
		uint16_t buffSize = 4 + txSize;
		uint8_t buff[buffSize];

		buff[0] = txSize >> 8;
		buff[1] = txSize;
		buff[2] = rxSize >> 8;
		buff[3] = rxSize;

		memcpy(&buff[4], tx, txSize);

		ret = this->cmdExecute(PROTO_CMD_SPI_TRANSFER, buff, buffSize, rx, rxSize, rxWritten);
		if (! ret) {
			break;
		}
	} while (0);

	return ret;
}
