#include <cstring>

#include "serialProgrammer.h"

#include "common/protocol.h"

#include "firmware/programmer.h"

#include "flashutil/exception.h"
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
			int        parametersSize;
			bool       hasResponse;
			uint8_t    code;
			ParseState stateOnOverflow;

			CmdDescription(uint8_t code, int parametersSize, bool hasResponse, ParseState stateOnOverflow) {
				this->parametersSize  = parametersSize;
				this->hasResponse     = hasResponse;
				this->code            = code;
				this->stateOnOverflow = stateOnOverflow;
			}
		};

	private:
		static const uint8_t ERASED_BYTE;
		static const uint8_t INVALID_CMD;

		static const uint8_t STATUS_FLAG_BUSY = 0x01;
		static const uint8_t STATUS_FLAG_WEL  = 0x02;

		static constexpr uint8_t CMD_RDID = 0x9f;
		static constexpr uint8_t CMD_SE   = 0x20;
		static constexpr uint8_t CMD_BE   = 0xd8;
		static constexpr uint8_t CMD_CE   = 0x60;
		static constexpr uint8_t CMD_RDSR = 0x05;
		static constexpr uint8_t CMD_WREN = 0x06;
		static constexpr uint8_t CMD_WRSR = 0x01;
		static constexpr uint8_t CMD_PP   = 0x02;
		static constexpr uint8_t CMD_RD   = 0x03;

	public:
		DummyFlash(const Flash &geometry) : geometry(geometry), memory(geometry.getSize(), ERASED_BYTE) {
			this->address        = 0;
			this->parseState     = ParseState::READ_CMD;
			this->cmdDescription = nullptr;
			this->statusReg      = geometry.getProtectMask();
			this->busyCounter    = 0;
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

				case CMD_RDSR:
					{
						ret = this->statusReg;
					}
					break;

				case CMD_RD:
					{
						if (! this->cmdData.empty()) {
							this->address = (this->cmdData[0] << 16) | (this->cmdData[1] << 8) | (this->cmdData[2]);

							this->cmdData.clear();
						}

						ret = this->memory[this->address];

						this->address = (this->address + 1) % this->memory.size();
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

			if (this->busyCounter > 0) {
				this->busyCounter--;

				if (this->busyCounter == 0) {
					this->statusReg &= ~STATUS_FLAG_BUSY;
					this->statusReg &= ~STATUS_FLAG_WEL;

					this->memory = this->memoryPending;

					HEX("Committed flash memory", this->memory.data(), this->memory.size());
				}
			}

			switch (this->parseState) {
				case ParseState::READ_CMD:
					{
						auto cmdDescIt = cmdsDescription.find(byte);
						if (cmdDescIt != cmdsDescription.end()) {
							this->cmdDescription = &cmdDescIt->second;

							if (this->cmdDescription->parametersSize) {
								pendingState = ParseState::READ_DATA;

							} else {
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
						if (this->cmdDescription->hasResponse) {
							ret = handleCmd(byte);

						} else {
							pendingState = this->cmdDescription->stateOnOverflow;
						}
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
			if (! select) {
				if (this->parseState == ParseState::HANDLE_CMD) {
					switch (this->cmdDescription->code) {
						case CMD_SE:
						case CMD_BE:
						case CMD_CE:
							{
								uint32_t address = 0;
								uint32_t size;

								if (this->cmdDescription->code == CMD_CE) {
									address = 0;
									size    = this->geometry.getSize();

								} else {
									address = (this->cmdData[0] << 16) | (this->cmdData[1] << 8) | (this->cmdData[2]);

									if (this->cmdDescription->code == CMD_BE) {
										size = this->geometry.getBlockSize();

									} else {
										size = this->geometry.getSectorSize();
									}
								}

								this->memoryPending = this->memory;

								std::fill(this->memoryPending.begin() + address, this->memoryPending.begin() + address + size, ERASED_BYTE);

								this->statusReg  |= STATUS_FLAG_BUSY;
								this->busyCounter = 40;
							}
							break;

						case CMD_WREN:
							{
								this->statusReg |= STATUS_FLAG_WEL;
							}
							break;

						case CMD_WRSR:
							{
								if ((this->statusReg & STATUS_FLAG_WEL) != 0) {
									this->statusReg = (this->cmdData[0] & 0xfc) | (this->statusReg & 0x03);
								}
							}
							break;

						case CMD_PP:
							{
								if ((this->statusReg & STATUS_FLAG_WEL) != 0) {
									uint32_t address = (this->cmdData[0] << 16) | (this->cmdData[1] << 8) | (this->cmdData[2]);

									this->memoryPending = this->memory;

									std::copy(this->cmdData.begin() + 3, this->cmdData.end(), this->memoryPending.begin() + address);

									this->statusReg  |= STATUS_FLAG_BUSY;
									this->busyCounter = 10;
								}
							}
							break;

						default:
							break;
					}
				}

				this->parseState     = ParseState::READ_CMD;
				this->cmdDescription = nullptr;
				this->cmdData.clear();
			}
		}

		const Flash &getGeometry() const {
			return this->geometry;
		}

	private:
		static std::map<uint8_t, CmdDescription> _initDescriptions() {
			std::map<uint8_t, CmdDescription> ret;

			ret.emplace(CMD_RDID, CmdDescription(CMD_RDID,     0, true,  ParseState::IGNORE));
			ret.emplace(CMD_CE,   CmdDescription(CMD_CE,       0, false, ParseState::IGNORE));
			ret.emplace(CMD_BE,   CmdDescription(CMD_BE,       3, false, ParseState::IGNORE));
			ret.emplace(CMD_SE,   CmdDescription(CMD_SE,       3, false, ParseState::IGNORE));
			ret.emplace(CMD_RDSR, CmdDescription(CMD_RDSR,     0, true,  ParseState::IGNORE));
			ret.emplace(CMD_WREN, CmdDescription(CMD_WREN,     0, false, ParseState::IGNORE));
			ret.emplace(CMD_WRSR, CmdDescription(CMD_WRSR,     1, false, ParseState::IGNORE));
			ret.emplace(CMD_PP,   CmdDescription(CMD_PP,      19, false, ParseState::HANDLE_CMD));
			ret.emplace(CMD_RD,   CmdDescription(CMD_RD,       3, true,  ParseState::HANDLE_CMD));

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
		uint8_t              statusReg;
		uint8_t              busyCounter;

		static std::map<uint8_t, CmdDescription> cmdsDescription;
};

constexpr uint8_t DummyFlash::CMD_RDID;
constexpr uint8_t DummyFlash::CMD_SE;
constexpr uint8_t DummyFlash::CMD_BE;
constexpr uint8_t DummyFlash::CMD_CE;
constexpr uint8_t DummyFlash::CMD_RDSR;
constexpr uint8_t DummyFlash::CMD_WREN;
constexpr uint8_t DummyFlash::CMD_WRSR;
constexpr uint8_t DummyFlash::CMD_PP;
constexpr uint8_t DummyFlash::CMD_RD;

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

	Impl(const Flash &flashInfo, size_t transferSize) : packetBuffer(transferSize), flash(flashInfo) {
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

	void write(void *buffer, std::size_t bufferSize, int timeoutMs) {
		uint8_t *bytes = reinterpret_cast<uint8_t *>(buffer);

		for (size_t i = 0; i < bufferSize; i++) {
			programmer_putByte(&this->programmer, bytes[i]);
		}
	}

	void read(void *buffer, std::size_t bufferSize, int timeoutMs) {
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

		TRACE("CALL");

		switch (request->cmd) {
			case PROTO_CMD_SPI_TRANSFER:
				{
					auto &req = request->request.transfer;
					auto &res = response->response.transfer;

					uint16_t toRecv = req.rxBufferSize;

					self->flash.cs(true);

					DEBUG("Request flags: %02x, tx: %u, rx: %u, skip: %u", req.flags, req.txBufferSize, req.rxBufferSize, req.rxSkipSize)
					HEX("Request data", req.txBuffer, req.txBufferSize);

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
						HEX("Response data", res.rxBuffer, res.rxBufferSize);
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

		TRACE("CALL");

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
