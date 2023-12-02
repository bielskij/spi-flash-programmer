#include <functional>
#include <algorithm>
#include <map>
#include <cstring>

#include "flashutil/exception.h"
#include "flashutil/programmer.h"
#include "flashutil/entryPoint.h"
#include "flashutil/debug.h"

using namespace flashutil;


using OperationHandler  = std::function<void(Programmer &programmer, const EntryPoint::Parameters &params)>;
using OperationHandlers = std::map<EntryPoint::Operation, std::map<EntryPoint::Mode, OperationHandler>>;


static bool isErased(Programmer &programmer, uint32_t startAddress, size_t size) {
	bool ret = true;

	{
		size_t bufferSize = programmer.getFlashInfo().getSectorSize();

		while (size > 0) {
			size_t toRead = std::min(size, bufferSize);

			auto buffer = programmer.read(startAddress, toRead);

			if (! std::all_of(buffer.begin(), buffer.end(), [](uint8_t v) { return v == 0xff; })) {
				ret = false;
				break;
			}

			startAddress += toRead;
			size         -= toRead;
		}
	}

	return ret;
}


static OperationHandlers _getHandlers() {
	OperationHandlers ret;

	{
		EntryPoint::Operation op;

		op = EntryPoint::Operation::ERASE;
		{
			ret[op][EntryPoint::Mode::CHIP]   =
			ret[op][EntryPoint::Mode::BLOCK]  =
			ret[op][EntryPoint::Mode::SECTOR] = [](Programmer &programmer, const EntryPoint::Parameters &params) {
				bool doErase = true;

				uint32_t address = params.index;
				size_t   size;

				std::function<void(Programmer &programmer)> operand;

				switch (params.mode) {
					case EntryPoint::Mode::CHIP:
						size    = programmer.getFlashInfo().getSize();
						operand = [](Programmer &prog) {
							prog.eraseChip();
						};
						break;

					case EntryPoint::Mode::BLOCK:
						size    = programmer.getFlashInfo().getBlockSize();
						operand = [&address](Programmer &prog) {
							prog.eraseBlockByAddress(address);
						};
						break;

					case EntryPoint::Mode::SECTOR:
						size    = programmer.getFlashInfo().getSectorSize();
						operand = [&address](Programmer &prog) {
							prog.eraseSectorByAddress(address);
						};
						break;

					default:
						address = 0;
						size    = 0;
				}

				if (size == 0) {
					OUT("Size of flash area to erase is 0 length. Flash will be not erased");

					doErase = false;
				}

				address *= size;

				if (doErase) {
					if (params.omitRedundantWrites) {
						INFO("Checking if flash area at %08x of size %u is already erased");

						if (isErased(programmer, address, size)) {
							INFO("Flash area is already erased. Skipping erase operation.");

							doErase = false;
						}
					}
				}

				if (doErase) {
					bool erased = true;

					INFO("Erasing flash area at %08x of size %u.", address, size);

					operand(programmer);

					if (params.verify) {
						if (! isErased(programmer, address, size)) {
							ERROR("Flash erase operation failed! Flash is not erased!");

							erased = false;
						}
					}

					if (erased) {
						INFO("Flash area erased.");
					}
				}
			};
		}

		op = EntryPoint::Operation::UNLOCK;
		{
			ret[op][EntryPoint::Mode::CHIP] = [](Programmer &programmer, const EntryPoint::Parameters &params) {
				uint8_t protectMask = programmer.getFlashInfo().getProtectMask();

				if (protectMask == 0) {
					throw_Exception("Unable to unprotect the chip. Protect mask is not defined!");
				}

				do {
					FlashStatus status = programmer.getFlashStatus();

					INFO("Unlocking flash chip.");

					if (params.omitRedundantWrites) {
						if (! status.isProtected(protectMask)) {
							INFO("Flash chip is already unlocked.");
							break;
						}
					}

					status.unprotect(protectMask);
					status = programmer.setFlashStatus(status);

					if (params.verify) {
						if (status.isProtected(protectMask)) {
							throw_Exception("Unable to unprotect flash chip! Please check if WP pin is correctly polarized!");
						}
					}

					INFO("Flash has been successfully unlocked.");
				} while (0);
			};
		}

		op = EntryPoint::Operation::WRITE;
		{
			ret[op][EntryPoint::Mode::SECTOR] =
			ret[op][EntryPoint::Mode::BLOCK]  =
			ret[op][EntryPoint::Mode::CHIP]   = [](Programmer &programmer, const EntryPoint::Parameters &params) {
				bool doWrite = true;

				uint32_t address = params.index;
				size_t   size;

				const Flash &flashInfo = programmer.getFlashInfo();

				switch (params.mode) {
					case EntryPoint::Mode::CHIP:
						size = programmer.getFlashInfo().getSize();
						break;

					case EntryPoint::Mode::BLOCK:
						size = flashInfo.getBlockSize();
						break;

					case EntryPoint::Mode::SECTOR:
						size = flashInfo.getSectorSize();
						break;

					default:
						break;
				}

				if (size == 0) {
					OUT("Size of flash area to write is 0 length. Flash will be not written!");

					doWrite = false;
				}

				address *= size;

				if (doWrite) {
					std::vector<uint8_t> page(flashInfo.getPageSize(), 0xff);

					while (! params.inStream->eof() && size > 0) {
						bool     pageWrite = true;
						uint32_t pageIdx   = address / flashInfo.getPageSize();

						params.inStream->read((char *) page.data(), page.size());

						size_t readSize = params.inStream->gcount();
						if (readSize == 0) {
							continue;
						}

						if (params.omitRedundantWrites) {
							auto readPage = programmer.read(address, readSize);

							if (std::equal(readPage.begin(), readPage.end(), page.begin())) {
								INFO("The page already contains the expected data. Skipping writing");

								pageWrite = false;

							} else if (! std::all_of(readPage.begin(), readPage.end(), [](uint8_t v) { return v == 0xff; })) {
								ERROR("The page is set to be written, but the flash page has not been yet erased. Skipping writing.");

								break;
							}
						}

						if (pageWrite) {
							if (readSize != page.size()) {
								memset(page.data() + readSize, 0xff, page.size() - readSize);
							}

							INFO("Writing page %u, in sector: %zd, in block %zd (addr %#08x).",
								pageIdx,
								(pageIdx * flashInfo.getPageSize()) / flashInfo.getSectorSize(),
								(pageIdx * flashInfo.getPageSize()) / flashInfo.getBlockSize(),
								address
							);

							programmer.writePage(address, page);

							if (params.verify) {
								auto readPage = programmer.read(address, readSize);

								if (! std::equal(readPage.begin(), readPage.end(), page.begin())) {
									ERROR("Verification of written page has failed! The page contains different data!");

									break;
								}
							}

							INFO("The page has been successfully written");
						}

						address += page.size();
						size    -= page.size();
					}
				}
			};
		}

		op = EntryPoint::Operation::READ;
		{
			ret[op][EntryPoint::Mode::SECTOR] =
			ret[op][EntryPoint::Mode::BLOCK]  =
			ret[op][EntryPoint::Mode::CHIP]   = [](Programmer &programmer, const EntryPoint::Parameters &params) {
				uint32_t address = params.index;
				size_t   read    = 0;
				size_t   size;

				const Flash &flashInfo = programmer.getFlashInfo();

				switch (params.mode) {
					case EntryPoint::Mode::CHIP:   size = flashInfo.getSize();       break;
					case EntryPoint::Mode::BLOCK:  size = flashInfo.getBlockSize();  break;
					case EntryPoint::Mode::SECTOR: size = flashInfo.getSectorSize(); break;

					default:
						break;
				}

				address *= size;

				INFO("Reading flash area of size %zd at %08x", size, address);

				while (read != size) {
					size_t toReadSize = std::min(flashInfo.getBlockSize(), size - read);

					auto readBuffer = programmer.read(address, toReadSize);

					params.outStream->write((char *) readBuffer.data(), readBuffer.size());

					address += toReadSize;
					read    += toReadSize;
				}
			};
		}
	}

	return ret;
}


static OperationHandler _getHandler(const EntryPoint::Parameters &params) {
	static OperationHandlers handlers = _getHandlers();

	auto opIt = handlers.find(params.operation);
	if (opIt != handlers.end()) {
		auto modeIt = opIt->second.find(params.mode);
		if (modeIt != opIt->second.end()) {
			return modeIt->second;
		}
	}

	return {};
}


void EntryPoint::call(Spi &spi, const FlashRegistry &registry, const Flash &flashGeometry, const Parameters &parameters) {
	call(spi, registry, flashGeometry, std::vector<Parameters>({ parameters }));
}


void EntryPoint::call(Spi &spi, const FlashRegistry &registry, const Flash &flashGeometry, const std::vector<Parameters> &parameters) {
	Programmer programmer(spi, &registry);

	programmer.begin(flashGeometry.isGeometryValid() ? &flashGeometry : nullptr);
	{
		for (const auto &p : parameters) {
			if (p.beforeExecution) {
				p.beforeExecution(p);
			}

			if (p.mode != Mode::NONE && p.operation != Operation::NO_OPERATION) {
				auto handler = _getHandler(p);
				if (handler) {
					handler(programmer, p);
				}

			} else {
				if (p.flashInfo != nullptr) {
					*p.flashInfo = programmer.getFlashInfo();
				}
			}

			if (p.afterExecution) {
				p.afterExecution(p);
			}
		}
	}
	programmer.end();
}


bool EntryPoint::Parameters::isValid() const {
	return true;
}
