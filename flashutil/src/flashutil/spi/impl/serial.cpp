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
	size_t               txSize;
	size_t               rxSize;

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

			size_t rxSize = msg.recv().getBytes();
			size_t txSize = msg.send().getBytes();
			size_t rxSkip = msg.recv().getSkips();

			size_t txWritten = 0;
			size_t rxWritten = 0;

			DEBUG("rxSize: %zd, txSize: %zd, skipSize: %zd", rxSize, txSize, rxSkip);

			while (rxSize > 0 || txSize > 0 || rxSkip > 0) {
				executeCmd(
					PROTO_CMD_SPI_TRANSFER,

					[&rxSize, &txSize, &rxSkip](ProtoReq &request, ProtoRes &response) {
						ProtoReqTransfer &t = request.request.transfer;

						t.txBufferSize = std::min((size_t) t.txBufferSize, txSize);
						txSize -= t.txBufferSize;

						if (txSize == 0) {
							t.rxSkipSize = rxSkip;
							rxSkip = 0;

							t.rxBufferSize = std::min((size_t) response.response.transfer.rxBufferSize, rxSize);
							rxSize -= t.rxBufferSize;

						} else {
							size_t remain = std::min(rxSkip, (size_t) t.txBufferSize);

							t.rxSkipSize = remain;
							rxSkip -= remain;

							if (t.txBufferSize > t.rxSkipSize) {
								remain = std::min(rxSize, (size_t) t.txBufferSize - t.rxSkipSize);

								t.rxBufferSize = remain;
								rxSize -= remain;
							}
						}

						if (rxSize > 0 || txSize > 0 || rxSkip > 0) {
							t.flags |= PROTO_SPI_TRANSFER_FLAG_KEEP_CS;
						}
					},

					[&txWritten, &msg](ProtoReq &request) {
						ProtoReqTransfer &t = request.request.transfer;

						memcpy(t.txBuffer, msg.send().data() + txWritten, t.txBufferSize);

						txWritten += t.txBufferSize;
					},

					[&rxWritten, &msg](const ProtoRes &response) {
						const ProtoResTransfer &t = response.response.transfer;

						memcpy(msg.recv().data() + rxWritten, t.rxBuffer, t.rxBufferSize);

						rxWritten += t.rxBufferSize;
					},

					TIMEOUT_MS
				);
			}
		}
	}


	void chipSelect(bool select) {
		ProtoRes response;

		this->executeCmd(PROTO_CMD_SPI_TRANSFER, [select](ProtoReq &req, ProtoRes &res) {
			ProtoReqTransfer &t = req.request.transfer;

			t.flags        = select ? PROTO_SPI_TRANSFER_FLAG_KEEP_CS : 0;
			t.rxBufferSize = 0;
			t.rxSkipSize   = 0;
			t.txBuffer     = nullptr;
			t.txBufferSize = 0;

		}, {}, {}, TIMEOUT_MS);
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
		std::function<void(ProtoReq &, ProtoRes &)> requestPrepareCallback,
		std::function<void(ProtoReq &)>             requestFillCallback,
		std::function<void(const ProtoRes &)>       responseDataCallback,
		int timeout
	) {
		uint8_t *packetBuffer     = this->packetBuffer.data();
		uint16_t packetBufferSize = this->packetBuffer.size();

		ProtoPkt packet;
		ProtoRes response;

		proto_pkt_init(&packet, packetBuffer, packetBufferSize, cmd, ++this->id);

		{
			ProtoReq request;

			proto_req_init(&request,  packet.payload, packet.payloadSize, packet.code);
			proto_res_init(&response, packet.payload, packet.payloadSize, packet.code);

			if (requestPrepareCallback) {
				requestPrepareCallback(request, response);
			}

			proto_pkt_prepare(&packet, packetBuffer, packetBufferSize, proto_req_getPayloadSize(&request));

			proto_req_assign(&request, packet.payload, packet.payloadSize);
			{
				if (requestFillCallback) {
					requestFillCallback(request);
				}
			}
			proto_req_encode(&request, packet.payload, packet.payloadSize);
		}

		HEX("Packet buffer", packetBuffer, packetBufferSize);

		this->serial->write(packetBuffer, proto_pkt_encode(&packet, packetBuffer, packetBufferSize), TIMEOUT_MS);

		{
			ProtoPktDes decoder;

			proto_pkt_dec_setup(&decoder, packetBuffer, packetBufferSize);

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

						proto_res_init  (&response, packet.payload, packet.payloadSize, cmd);
						proto_res_decode(&response, packet.payload, packet.payloadSize);
						proto_res_assign(&response, packet.payload, packet.payloadSize);

						if (responseDataCallback) {
							responseDataCallback(response);
						}
					}
				} while (decRet == PROTO_PKT_DES_RET_IDLE);
			}
		}
	}

	void attach() {
		executeCmd(PROTO_CMD_GET_INFO, {}, {}, [this](const ProtoRes &response) {
			DEBUG("version %hhu.%hhu, payload size: %hu", response.response.getInfo.version.major, response.response.getInfo.version.minor, response.response.getInfo.packetSize);

			this->packetBuffer = std::vector<uint8_t>(response.response.getInfo.packetSize);
		}, TIMEOUT_MS);

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
