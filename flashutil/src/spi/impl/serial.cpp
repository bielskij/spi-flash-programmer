#include <cstring>

#include "crc8.h"
#include "protocol.h"

#include "exception.h"

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

	// Be sure CS pin is released.
	this->chipSelect(false);
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

		uint8_t txBuffer[txSize];
		uint8_t rxBuffer[rxSize];

		memcpy(txBuffer, msg.send().data(), txSize);

		if (rxSize) {
			memset(rxBuffer, 0, rxSize);
		}

		if (msg.isAutoChipSelect()) {
			this->spiCs(false);
		}

		this->spiTransfer(txBuffer, txSize, rxBuffer, rxSize);

		{
			std::size_t dstOffset = 0;

			auto skipIndex = msg.recv().getSkipMap();

			for (int i = 0; i < rxSize; i++) {
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

#define TRANSFER_DATA_BLOCK_SIZE ((size_t) 240)

void SerialSpi::cmdExecute(uint8_t cmd, uint8_t *data, size_t dataSize, uint8_t *response, size_t responseSize) {
	size_t triesLeft = 3;
	bool   success   = false;

	DBG(("CALL cmd: %d (%02x), %p, %zd, %p, %zd", cmd, cmd, data, dataSize, response, responseSize));

	do {
		triesLeft--;

		try {
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

		//		debug_dumpBuffer(txData, txDataSize, 32, 0);

				self->serial->write(txData, txDataSize, TIMEOUT_MS);
			}

			// Receive
			{
				uint8_t rxHeader[4];

				self->serial->read(rxHeader, sizeof(rxHeader), TIMEOUT_MS);

				// Sync
				if (rxHeader[0] != PROTO_SYNC_BYTE) {
					throw_Exception("Received byte is not a sync byte!");
				}

				{
					size_t  rxDataSize = ((rxHeader[2] << 8) | rxHeader[3]) + 1; // room for crc
					uint8_t rxData[rxDataSize];

					self->serial->read(rxData, rxDataSize, TIMEOUT_MS);

					DBG(("rxDataSize: %zd", rxDataSize));

					// Check CRC
					{
						uint8_t crc = PROTO_CRC8_START;

						crc = crc8_get(rxHeader, sizeof(rxHeader), PROTO_CRC8_POLY, crc);
						crc = crc8_get(rxData, rxDataSize - 1, PROTO_CRC8_POLY, crc);

						if (crc != rxData[rxDataSize - 1]) {
							throw_Exception("CRC mismatch!");
						}
					}

					if (response) {
						size_t toCopy = std::min(responseSize, rxDataSize - 1);

						if (toCopy) {
							memcpy(response, rxData, toCopy);
						}
					}
				}

				{
					uint8_t code = rxHeader[1];

					if (code != PROTO_NO_ERROR) {
						throw_Exception("Received error! " + std::to_string(code));
					}
				}
			}

			success = true;
		} catch (const Exception &ex) {
			PRINTFLN(("Got exception! %s - %zd tries left", ex.what(), triesLeft));

			if (triesLeft == 0) {
				throw;
			}
		}
	} while (! success);

//	debug_dumpBuffer(response, responseSize, 32, 0);
}


void SerialSpi::spiCs(bool high) {
	this->cmdExecute(high ? PROTO_CMD_SPI_CS_HI : PROTO_CMD_SPI_CS_LO, NULL, 0, NULL, 0);
}


void SerialSpi::spiTransfer(uint8_t *tx, uint16_t txSize, uint8_t *rx, uint16_t rxSize) {
	size_t totalSize      = std::max(txSize, rxSize);
	size_t rxProcessed    = 0;
	size_t txProcessed    = 0;
	size_t totalProcessed = 0;

	const size_t dataMaxSize = TRANSFER_DATA_BLOCK_SIZE - 4;

	while (totalProcessed != totalSize) {
		size_t turnSize = std::min(dataMaxSize, totalSize - totalProcessed);

		size_t turnTx = std::min(turnSize, txSize - txProcessed);
		size_t turnRx = std::min(turnSize, rxSize - rxProcessed);

		size_t  buffSize = 4 + turnTx;
		uint8_t buff[buffSize];

		buff[0] = turnTx >> 8;
		buff[1] = turnTx;
		buff[2] = turnRx >> 8;
		buff[3] = turnRx;

		if (turnTx) {
			memcpy(&buff[4], tx + txProcessed, turnTx);
		}

		this->cmdExecute(PROTO_CMD_SPI_TRANSFER, buff, buffSize, rx + rxProcessed, turnRx);

		txProcessed += turnTx;
		rxProcessed += turnRx;

		totalProcessed += turnSize;
	}

	DBG(("totalSize: %zd, txProcessed: %zd, rxProcessed: %zd, totalPorcessed: %zd", totalSize, txProcessed, rxProcessed, totalProcessed));
}
