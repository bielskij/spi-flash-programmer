#include <cstring>

#include "serialProgrammer.h"

#include "common/protocol.h"
#include "firmware/programmer.h"
#include "flashutil/exception.h"

#define DEBUG 1
#include "flashutil/debug.h"


class DummyFlash {
	private:
		enum class ParseState {
			READ_CMD,
			READ_DATA,
			HANDLE_CMD,
			WRITE_RESPONSE,
			IGNORE
		};

		struct CmdDescription {
			int     parametersSize;
			bool    hasResponse;
			uint8_t code;

			CmdDescription(uint8_t code, int parametersSize, bool hasResponse) {
				this->parametersSize = parametersSize;
				this->hasResponse    = hasResponse;
				this->code           = code;
			}
		};

	private:
		static const uint8_t ERASED_BYTE;
		static const uint8_t INVALID_CMD;

		static const uint8_t STATUS_FLAG_BUSY = 0x01;
		static const uint8_t STATUS_FLAG_WEL  = 0x02;

		static constexpr uint8_t CMD_RDID = 0x9f;
		static constexpr uint8_t CMD_SE   = 0x20;
		static constexpr uint8_t CMD_BE   = 0x52;
		static constexpr uint8_t CMD_CE   = 0x60;

	public:
		DummyFlash(const Flash &geometry) : geometry(geometry), memory(geometry.getSize(), ERASED_BYTE) {
			this->address        = 0;
			this->parseState     = ParseState::READ_CMD;
			this->cmdDescription = nullptr;
		}

		uint8_t handleCmd(uint8_t byte) {
			uint8_t ret = 0xff;

			switch (this->cmdDescription->code) {
				case CMD_RDID:
					{
						if (this->address < 3) {
							ret = this->geometry.getId()[this->address++];
						}

						if (this->address == 3) {
							this->parseState = ParseState::IGNORE;
						}
					}
					break;

				default:
					break;
			}

			return ret;
		}

		uint8_t consumeByte(uint8_t byte) {
			uint8_t ret = 0xff;

			ParseState pendingState = this->parseState;

			switch (this->parseState) {
				case ParseState::READ_CMD:
					{
						auto cmdDescIt = cmdsDescription.find(byte);
						if (cmdDescIt != cmdsDescription.end()) {
							this->cmdDescription = &cmdDescIt->second;

							if (this->cmdDescription->parametersSize) {
								pendingState = ParseState::READ_DATA;

							} else if (this->cmdDescription->hasResponse) {
								pendingState = ParseState::HANDLE_CMD;
							}
						}
					}
					break;

				case ParseState::READ_DATA:
					{
						this->cmdData.push_back(byte);
						if (this->cmdData.size() == this->cmdDescription->parametersSize) {
							pendingState = ParseState::HANDLE_CMD;
						}
					}
					break;

				case ParseState::HANDLE_CMD:
					{
						ret = handleCmd(byte);
					}
					break;

				case ParseState::IGNORE:
					break;
			}

			if (pendingState != this->parseState) {
				this->address    = 0;
				this->parseState = pendingState;
			}

			return ret;
		}

		uint8_t transfer(uint8_t txByte) {
			return this->consumeByte(txByte);
		}

		void cs(bool select) {
			this->parseState     = ParseState::READ_CMD;
			this->cmdDescription = nullptr;
			this->cmdData.clear();
		}

		const Flash &getGeometry() const {
			return this->geometry;
		}

	private:
		static std::map<uint8_t, CmdDescription> _initDescriptions() {
			std::map<uint8_t, CmdDescription> ret;

			ret.emplace(CMD_RDID, CmdDescription(CMD_RDID, 0, true));
			ret.emplace(CMD_CE,   CmdDescription(CMD_CE,   0, false));
			ret.emplace(CMD_BE,   CmdDescription(CMD_BE,   3, false));
			ret.emplace(CMD_SE,   CmdDescription(CMD_SE,   3, false));

			return ret;
		}

	private:
		Flash                geometry;
		size_t               address;
		std::vector<uint8_t> memory;
		std::vector<uint8_t> memoryPending;

		ParseState           parseState;
		std::vector<uint8_t> cmdData;
		CmdDescription      *cmdDescription;

		static std::map<uint8_t, CmdDescription> cmdsDescription;
};

constexpr uint8_t DummyFlash::CMD_RDID;
constexpr uint8_t DummyFlash::CMD_SE;
constexpr uint8_t DummyFlash::CMD_BE;
constexpr uint8_t DummyFlash::CMD_CE;

const uint8_t DummyFlash::ERASED_BYTE = 0xff;
const uint8_t DummyFlash::INVALID_CMD = 0xff;
std::map<uint8_t, DummyFlash::CmdDescription> DummyFlash::cmdsDescription = DummyFlash::_initDescriptions();


struct SerialProgrammer::Impl {
	Spi::Config          config;
	Spi::Capabilities    capabilities;
	Programmer           programmer;
	std::vector<uint8_t> packetBuffer;
	std::vector<uint8_t> outputBuffer;
	DummyFlash           flash;

	Impl(const Flash &flashInfo, size_t transferSize) : packetBuffer(16), flash(flashInfo) {
		this->capabilities.setTransferSizeMax(transferSize);

		programmer_setup(
			&this->programmer,
			this->packetBuffer.data(),
			this->packetBuffer.size(),
			_programmerRequestCallback,
			_programmerResponseCallback,
			this
		);
	}

	const Flash &getFlashInfo() {
		return this->flash.getGeometry();
	}

	void write(void *buffer, std::size_t bufferSize, int timeoutMs) {
		uint8_t *bytes = reinterpret_cast<uint8_t *>(buffer);

		DBG(("CALL for %zd bytes", bufferSize));

		for (size_t i = 0; i < bufferSize; i++) {
			programmer_putByte(&this->programmer, bytes[i]);
		}
	}

	void read(void *buffer, std::size_t bufferSize, int timeoutMs) {
		DBG(("CALL, have %zd bytes, requested %zd bytes", this->outputBuffer.size(), bufferSize));

		if (outputBuffer.size() < bufferSize) {
			throw_Exception("Not enough data in buffer!");
		}

		auto beg = this->outputBuffer.begin();
		auto end = beg + bufferSize;

		std::copy(beg, end, (uint8_t *) buffer);

		this->outputBuffer.erase(beg, end);
	}

	static void _programmerRequestCallback(ProtoReq *request, ProtoRes *response, void *callbackData) {
		Impl *self = reinterpret_cast<Impl *>(callbackData);

		DBG(("CALL"));

		switch (request->cmd) {
			case PROTO_CMD_SPI_TRANSFER:
				{
					auto &req = request->request.transfer;
					auto &res = response->response.transfer;

					uint16_t toRecv = req.rxBufferSize;

					self->flash.cs(true);

					debug_dumpBuffer(req.txBuffer, req.txBufferSize, 32, 0);

					for (uint16_t i = 0; i < std::max(req.txBufferSize, (uint16_t)(req.rxSkipSize + req.rxBufferSize)); i++) {
						uint8_t received;

						if (i < req.txBufferSize) {
							received = self->flash.transfer(req.txBuffer[i]);

						} else {
							received = self->flash.transfer(0xff);
						}

						if (toRecv) {
							if (i >= req.rxSkipSize) {
								res.rxBuffer[i - req.rxSkipSize] = received;

								toRecv--;
							}
						}
					}

					if (res.rxBufferSize > 0) {
						debug_dumpBuffer(res.rxBuffer, res.rxBufferSize, 32, 0);
					}

					if ((req.flags & PROTO_SPI_TRANSFER_FLAG_KEEP_CS) == 0) {
						self->flash.cs(false);
					}
				}
				break;

			default:
				break;
		}
	}

	static void _programmerResponseCallback(uint8_t *buffer, uint16_t bufferSize, void *callbackData) {
		Impl *self = reinterpret_cast<Impl *>(callbackData);

		DBG(("CALL"));

		std::copy(buffer, buffer + bufferSize, std::back_inserter(self->outputBuffer));
	}
};


SerialProgrammer::SerialProgrammer(const Flash &flashInfo, size_t transferSize) {
	this->_self = std::make_unique<Impl>(flashInfo, transferSize);
}


SerialProgrammer::~SerialProgrammer() {
}


void SerialProgrammer::write(void *buffer, std::size_t bufferSize, int timeoutMs) {
	this->_self->write(buffer, bufferSize, timeoutMs);
}


void SerialProgrammer::read(void *buffer, std::size_t bufferSize, int timeoutMs) {
	this->_self->read(buffer, bufferSize, timeoutMs);
}


const Flash &SerialProgrammer::getFlashInfo() {
	return this->_self->getFlashInfo();
}
