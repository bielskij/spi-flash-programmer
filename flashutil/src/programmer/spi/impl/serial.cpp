/*
 * SerialSpi.cpp
 *
 *  Created on: 5 gru 2022
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#include <cstdlib>

#include "protocol.h"
#include "crc8.h"

#if defined(__unix__)
#include <fcntl.h>
#include <termios.h>
#endif

#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include "serial.h"

#include "common/debug.h"

#define TIMEOUT_MS 1000


struct flashutil::SerialSpi::Impl {
	bool serialWrite(void *buffer, size_t bufferSize, int timeoutMs) {
		return false;
	}

	bool serialReadByte(void *value, int timeoutMs) {
		return false;
	}

	bool callCommand(uint8_t cmd, const Buffer &tx, Buffer *rx) {
		bool ret;

		/*
		 * [FRAME_TYPE]
		 * [PROTO_CMD]
		 * [LEN_HI]
		 * [LEN_LO]
		 *
		 * SPI transfer length (rx and tx)
		 * [TX_LEN_HI]
		 * [TX_LEN_LO]
		 * [RX_LEN_HI]
		 * [RX_LEN_LO]
		 *
		 * Data to send over SPI, N=TX_LEN - 1
		 * [TX_DATA_0]
		 *     ...
		 * [TX_DATA_N]
		 *
		 * [CRC8]
		 */
		do {
			// Send
			{
				Buffer cmdData;

				{
					bool sendSpiFrame = false;

					if (tx.size() || (rx && rx->size())) {
						sendSpiFrame = true;
					}

					{
						size_t transferSize = 4 + 1; // header + CRC

						if (tx.size() || (rx && rx->size())) {
							transferSize += (4 + tx.size());
						}

						cmdData.resize(transferSize);
					}

					{
						uint32_t len = sendSpiFrame ? 4 + tx.size() : 0;
						uint32_t off = 0;

						cmdData[off++] = PROTO_SYNC_BYTE;
						cmdData[off++] = cmd;

						cmdData[off++] = len >> 8;
						cmdData[off++] = len & 0xff;

						if (sendSpiFrame) {
							len = tx.size();

							cmdData[off++] = len >> 8;
							cmdData[off++] = len & 0xff;

							len = rx ? rx->size() : 0;

							cmdData[off++] = len >> 8;
							cmdData[off++] = len & 0xff;

							for (size_t i = 0; i < tx.size(); i++) {
								cmdData[off + i] = tx[i];
							}

							off += tx.size();
						}

						cmdData[off] = crc8_get(cmdData.data(), cmdData.size() - 1, PROTO_CRC8_POLY, PROTO_CRC8_START);
					}
				}

				ret = this->serialWrite(cmdData.data(), cmdData.size(), TIMEOUT_MS);
				if (! ret) {
					break;
				}
			}

			// Receive
			{
				uint8_t crc = PROTO_CRC8_START;
				uint8_t tmp;

				// sync
				{
					ret = this->serialReadByte(&tmp, TIMEOUT_MS);
					if (! ret) {
						break;
					}

					ret = tmp == PROTO_SYNC_BYTE;
					if (! ret) {
						ERR(("Received byte is not a sync byte!"));
						break;
					}

					crc = crc8_getForByte(tmp, PROTO_CRC8_POLY, crc);
				}

				// code
				{
					ret = this->serialReadByte(&tmp, TIMEOUT_MS);
					if (! ret) {
						break;
					}

					if (tmp != PROTO_NO_ERROR) {
						ERR(("Received error! (%#02x)", tmp));
					}

					crc = crc8_getForByte(tmp, PROTO_CRC8_POLY, crc);
				}

				// length
				{
					uint16_t dataLen = 0;

					for (int i = 0; i < 2; i++) {
						ret = this->serialReadByte(&tmp, TIMEOUT_MS);
						if (! ret) {
							break;
						}

						dataLen |= (tmp << (8 * (1 - i)));

						crc = crc8_getForByte(tmp, PROTO_CRC8_POLY, crc);
					}

					if (! ret) {
						break;
					}

					if (dataLen != rx->size()) {
						ERR(("Received less bytes than expected!"));

						ret = false;
						break;
					}

					// Data
					for (uint16_t i = 0; i < dataLen; i++) {
						ret = this->serialReadByte(&tmp, TIMEOUT_MS);
						if (! ret) {
							break;
						}

						crc = crc8_getForByte(tmp, PROTO_CRC8_POLY, crc);

						rx[i] = tmp;
					}

					if (! ret) {
						break;
					}
				}

				// CRC
				{
					ret = this->serialReadByte(&tmp, TIMEOUT_MS);
					if (! ret) {
						break;
					}

					ret = tmp == crc;
					if (! ret) {
						ERR(("CRC mismatch!"));
						break;
					}
				}
			}
		} while (0);

		if (! ret) {
			if (rx) {
				rx->fill(0x00);
			}
		}

		return ret;
	}
};


bool flashutil::SerialSpi::chipSelect() {
	return self->callCommand(PROTO_CMD_SPI_CS_LO, Buffer(), nullptr);
}


bool flashutil::SerialSpi::chipDeselect() {
	return self->callCommand(PROTO_CMD_SPI_CS_HI, Buffer(), nullptr);
}


flashutil::SerialSpi::SerialSpi(const std::string &serial) {

}


flashutil::SerialSpi::~SerialSpi() {
}


bool flashutil::SerialSpi::transfer(const Buffer &tx, Buffer *rx) {
	return self->callCommand(PROTO_CMD_SPI_TRANSFER, tx, rx);
}
