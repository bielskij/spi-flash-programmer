#include <cstring>
#include <functional>

#include "common/crc8.h"
#include "common/protocol.h"
#include "common/protocol/packet.h"
#include "common/protocol/request.h"
#include "common/protocol/response.h"

#include "flashutil/exception.h"

#include "serial.h"
#include "serial/serial.h"

#define DEBUG 1
#include "flashutil/debug.h"


#define TIMEOUT_MS 1000

#define TRANSFER_DATA_BLOCK_SIZE ((size_t) 251)


struct SerialProxy : public Serial {
	public:
		SerialProxy(Serial &serial) : _serial(serial) {
		}

		virtual void write(void *buffer, std::size_t bufferSize, int timeoutMs) override {
			this->_serial.write(buffer, bufferSize, timeoutMs);
		}

		virtual void read(void *buffer, std::size_t bufferSize, int timeoutMs) override {
			this->_serial.read(buffer, bufferSize, timeoutMs);
		}

	private:
		Serial &_serial;
};


struct SerialSpi::Impl {
	std::unique_ptr<Serial> serial;
	Config                  config;
	uint8_t                 id;
	Capabilities            capabilities;

	std::vector<uint8_t> packetBuffer;

	Impl(Serial &serial) : packetBuffer(32) {
		this->serial.reset(new SerialProxy(serial));

		this->init(true);
	}

	Impl(const std::string &path, int baudrate) {
		this->serial.reset(new HwSerial(path, baudrate));

		this->init(false);
	}

	void init(bool attached) {
		this->id = 0;
	}

	void transfer(Messages &msgs) {
		for (size_t i = 0; i < msgs.count(); i++) {
			auto &msg = msgs.at(i);

			const size_t rxSize = msg.recv().getBytes();
			const size_t txSize = msg.send().getBytes();
			const size_t rxSkip = txSize + msg.recv().getSkips();

			uint8_t txBuffer[txSize];
			uint8_t rxBuffer[rxSize];

			memcpy(txBuffer, msg.send().data(), txSize);

			if (rxSize) {
				memset(rxBuffer, 0, rxSize);
			}

			this->spiTransfer(txBuffer, txSize, rxBuffer, rxSize, rxSkip, 0);

			if (msg.recv().getSkipMap().size() > 1) {
				throw std::runtime_error("More than 1 skip operation in a single message is not yet supported!");
			}

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
		}
	}


	void chipSelect(bool select) {
		ProtoRes response;

		this->executeCmd(PROTO_CMD_SPI_TRANSFER, [select](ProtoReq &req) {
			ProtoReqTransfer &t = req.request.transfer;

			t.flags        = select ? PROTO_SPI_TRANSFER_FLAG_KEEP_CS : 0;
			t.rxBufferSize = 0;
			t.rxSkipSize   = 0;
			t.txBuffer     = nullptr;
			t.txBufferSize = 0;

		}, response, TIMEOUT_MS);
	}


	Config getConfig() {
		return this->config;
	}


	void setConfig(const Config &config) {
		this->config = config;
	}


	void cmdExecute(uint8_t cmd, uint8_t *data, size_t dataSize, uint8_t *response, size_t responseSize) {
//		size_t triesLeft = this->config.getRetransmissions();
//		bool   success   = false;

		DBG(("CALL cmd: %d (%02x), %p, %zd, %p, %zd", cmd, cmd, data, dataSize, response, responseSize));

//		do {
//			if (triesLeft) {
//				triesLeft--;
//			}
//
//			try {
	#if 0
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
	#endif
//				success = true;
//			} catch (const Exception &ex) {
//	//			PRINTFLN(("Got exception! %s - %zd tries left", ex.what(), triesLeft));
//
//				if (triesLeft == 0) {
//					throw;
//				}
//			}
//		} while (! success);

	//	debug_dumpBuffer(response, responseSize, 32, 0);
	}


	void spiTransfer(uint8_t *tx, uint16_t txSize, uint8_t *rx, uint16_t rxSize, uint16_t rxSkip, uint8_t flags) {
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

	const Capabilities &getCapabilities() const {
		return this->capabilities;
	}

	void executeCmd(uint8_t cmd, std::function<void(ProtoReq &)> requestSetupCallback, ProtoRes &response, int timeout) {
		ProtoPkt packet;

		{
			ProtoReq request;

			proto_req_init(&request, cmd, ++this->id);

			proto_pkt_init(
				&packet,
				this->packetBuffer.data(),
				this->packetBuffer.size(),
				request.cmd,
				request.id,
				proto_req_getPayloadSize(&request)
			);

			proto_req_assign(&request, packet.payload, packet.payloadSize);
			if (requestSetupCallback) {
				requestSetupCallback(request);
			}
			proto_req_encode(&request, packet.payload, packet.payloadSize);
		}

		this->serial->write(packetBuffer.data(), proto_pkt_encode(&packet), TIMEOUT_MS);

		{
			ProtoPktDes decoder;

			proto_pkt_dec_setup(&decoder, this->packetBuffer.data(), this->packetBuffer.size());

			{
				uint8_t decRet;

				do {
					uint8_t byte;

					this->serial->read(&byte, 1, TIMEOUT_MS);

					decRet = proto_pkt_dec_putByte(&decoder, byte, &packet);

					if (decRet != PROTO_PKT_DES_RET_IDLE) {
						if (PROTO_PKT_DES_RET_GET_ERROR_CODE(decRet) != PROTO_NO_ERROR) {
							throw_Exception("Protocol error! " + PROTO_PKT_DES_RET_GET_ERROR_CODE(decRet));
						}

						if (packet.id != this->id) {
							throw_Exception("Protocol error! ID does not match!");
						}

						proto_res_init(&response, cmd, packet.code, packet.id);

						proto_res_decode(&response, packet.payload, packet.payloadSize);
						proto_res_assign(&response, packet.payload, packet.payloadSize);
					}
				} while (decRet == PROTO_PKT_DES_RET_IDLE);
			}
		}
	}

	void attach() {
		ProtoRes response;

		executeCmd(PROTO_CMD_GET_INFO, {}, response, TIMEOUT_MS);

		DBG(("version %u.%u, payload size: %u", response.response.getInfo.version.major, response.response.getInfo.version.minor, response.response.getInfo.payloadSize));

		// Be sure CS pin is released.
		this->chipSelect(false);
	}


	void detach() {
		// Be sure CS pin is released.
		this->chipSelect(false);
	}
};


SerialSpi::SerialSpi(const std::string &path, int baudrate) {
	this->self.reset(new SerialSpi::Impl(path, baudrate));
}


SerialSpi::SerialSpi(Serial &serial) {
	this->self.reset(new SerialSpi::Impl(serial));
}


SerialSpi::~SerialSpi() {

}


void SerialSpi::chipSelect(bool select) {
	this->self->chipSelect(select);
}


Spi::Config SerialSpi::getConfig() {
	return self->getConfig();
}


void SerialSpi::setConfig(const Config &config) {
	self->setConfig(config);
}


void SerialSpi::transfer(Messages &msgs) {
	self->transfer(msgs);
}


const Spi::Capabilities &SerialSpi::getCapabilities() const {
	return self->getCapabilities();
}


void SerialSpi::attach() {
	self->attach();
}


void SerialSpi::detach() {
	self->detach();
}
