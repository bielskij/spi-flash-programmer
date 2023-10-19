#include "serialProgrammer.h"

#include "common/protocol.h"
#include "firmware/programmer.h"

#define DEBUG 1
#include "flashutil/debug.h"


class DummyFlash {
	private:
		enum class ParseState {
			READ_CMD,
			READ_DATA,
			HANDLE_CMD,
			IGNORE
		};

		struct CmdDescription {
			int parametersSize;

			CmdDescription(int parametersSize) {
				this->parametersSize = parametersSize;
			}
		};

	private:
		static const uint8_t ERASED_BYTE;
		static const uint8_t INVALID_CMD;

		static const uint8_t STATUS_FLAG_BUSY = 0x01;
		static const uint8_t STATUS_FLAG_WEL  = 0x02;

		static const uint8_t CMD_ERASE_SECTOR = 0x20;
		static const uint8_t CMD_ERASE_BLOCK  = 0x52;
		static const uint8_t CMD_ERASE_CHIP   = 0x60;

	public:
		DummyFlash(const Flash &geometry) : geometry(geometry), memory(geometry.getSize(), ERASED_BYTE) {
			this->address    = 0;
			this->parseState = ParseState::READ_CMD;
			this->cmd        = INVALID_CMD;
		}

		uint8_t handleCmd(uint8_t byte) {
			uint8_t ret = 0xff;

			switch (this->cmd) {
				default:
					break;
			}

			return ret;
		}

		uint8_t consumeByte(uint8_t byte) {
			uint8_t ret = 0xff;

			switch (this->parseState) {
				case ParseState::READ_CMD:
					{
						this->cmd = byte;

						this->parseState = ParseState::READ_DATA;
					}
					break;

				case ParseState::READ_DATA:
					{
						this->cmdData.push_back(byte);
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

			return ret;
		}

		uint8_t onNewCmdByte(uint8_t cmd, const std::vector<uint8_t> &data) {
			uint8_t ret = 0xff;

			switch (cmd) {
				case CMD_ERASE_SECTOR:
				case CMD_ERASE_BLOCK:
					if (data.size() == 3) {
						uint32_t address = (data[0] << 2) | (data[1] << 1) | (data[2]);
						size_t   toErase = cmd == CMD_ERASE_SECTOR ? this->geometry.getSectorSize() : this->geometry.getBlockSize();

						std::fill_n(this->memory.begin() + address, toErase, ERASED_BYTE);
					}
					break;

				case CMD_ERASE_CHIP:
					if (data.size() == 0) {
						std::fill_n(this->memory.begin(), this->memory.size(), ERASED_BYTE);
						break;
					}
					break;

				default:
					break;
			}

			return ret;
		}

		void transfer(uint8_t *buffer, size_t bufferSize) {
			for (size_t i = 0; i < bufferSize; i++) {
				uint8_t byte = buffer[i];

				switch (this->parseState) {
					case ParseState::READ_CMD:
						{
							this->cmd = byte;

							this->parseState = ParseState::READ_DATA;
						}
						break;

					case ParseState::READ_DATA:
						{
							this->cmdData.push_back(byte);
						}
						break;
				}

				buffer[i] = onNewCmdByte(this->cmd, this->cmdData);
			}
		}

		void cs(bool select) {
			if (! select) {
				this->handleCmd(0);

			} else {
				this->parseState = ParseState::READ_CMD;

				this->cmdData.clear();
			}
		}

		const Flash &getGeometry() const {
			return this->geometry;
		}

	private:
		static std::map<uint8_t, CmdDescription> _initDescriptions() {
			std::map<uint8_t, CmdDescription> ret;

			ret[CMD_ERASE_CHIP]   = CmdDescription(0);
			ret[CMD_ERASE_BLOCK]  = CmdDescription(3);
			ret[CMD_ERASE_SECTOR] = CmdDescription(3);

			return ret;
		}

	private:
		Flash                geometry;
		size_t               address;
		std::vector<uint8_t> memory;
		std::vector<uint8_t> memoryPending;

		ParseState           parseState;
		uint8_t              cmd;
		std::vector<uint8_t> cmdData;

		static std::map<uint8_t, CmdDescription> cmdsDescription;
};


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
			throw std::runtime_error("Not enough data in buffer!");
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
					auto &t = request->request.transfer;

					self->flash.cs(true);



					if ((t.flags & PROTO_SPI_TRANSFER_FLAG_KEEP_CS) == 0) {
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
