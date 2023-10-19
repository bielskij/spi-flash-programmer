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
		ProtoPkt pkt;

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

			this->executeCmd(
				PROTO_CMD_SPI_TRANSFER,
				[](ProtoReq &request) {
					request.request.transfer.txBufferSize
				},
				[](ProtoRes &response) {

				},
				TIMEOUT_MS
			);

//			this->spiTransfer(txBuffer, txSize, rxBuffer, rxSize, rxSkip, 0);
//
//			if (msg.recv().getSkipMap().size() > 1) {
//				throw std::runtime_error("More than 1 skip operation in a single message is not yet supported!");
//			}
//
//			{
//				std::size_t dstOffset = 0;
//
//				auto skipIndex = msg.recv().getSkipMap();
//
//				for (int i = 0; i < rxSize; i++) {
//					if (skipIndex.find(i) != skipIndex.end()) {
//						continue;
//					}
//
//					msg.recv().data()[dstOffset++] = rxBuffer[i];
//				}
//			}
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

		}, {}, TIMEOUT_MS);
	}


	Config getConfig() {
		return this->config;
	}


	void setConfig(const Config &config) {
		this->config = config;
	}

	const Capabilities &getCapabilities() const {
		return this->capabilities;
	}

	void executeCmd(
		uint8_t cmd,
		std::function<void(ProtoReq &)> requestPrepareCallback,
		std::function<void(ProtoReq &)> requestFillCallback,
		std::function<void(ProtoRes &)> responseDataCallback,
		int timeout
	) {
		ProtoPkt packet;

		{
			ProtoReq request;

			proto_req_init(&request, cmd, ++this->id);

			if (requestPrepareCallback) {
				requestPrepareCallback(request);
			}

			proto_pkt_init(
				&packet,
				this->packetBuffer.data(),
				this->packetBuffer.size(),
				request.cmd,
				request.id,
				proto_req_getPayloadSize(&request)
			);

			proto_req_assign(&request, packet.payload, packet.payloadSize);
			{
				if (requestFillCallback) {
					requestFillCallback(request);
				}
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

						{
							ProtoRes response;

							proto_res_init(&response, cmd, packet.code, packet.id);

							proto_res_decode(&response, packet.payload, packet.payloadSize);
							proto_res_assign(&response, packet.payload, packet.payloadSize);

							if (responseDataCallback) {
								responseDataCallback(response);
							}
						}
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
