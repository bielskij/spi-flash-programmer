#include <functional>
#include <map>
#include <cstring>

#include "flashutil/exception.h"
#include "flashutil/programmer.h"
#include "flashutil/entryPoint.h"
#include "flashutil/debug.h"

using namespace flashutil;


const int EntryPoint::RC_SUCCESS = 0;
const int EntryPoint::RC_FAILURE = 1;

using OperationHandler  = std::function<void(Programmer &programmer, const EntryPoint::Parameters &params)>;
using OperationHandlers = std::map<EntryPoint::Operation, std::map<EntryPoint::Mode, OperationHandler>>;


static OperationHandlers _getHandlers() {
	OperationHandlers ret;

	{
		EntryPoint::Operation op;

		op = EntryPoint::Operation::ERASE;
		{
			ret[op][EntryPoint::Mode::CHIP] = [](Programmer &programmer, const EntryPoint::Parameters &params) {
				INFO("Erasing whole flash chip.", params.index);

				programmer.eraseChip();

				INFO("Flash chip erased.");
			};

			ret[op][EntryPoint::Mode::BLOCK] = [](Programmer &programmer, const EntryPoint::Parameters &params) {
				INFO("Erasing block %d.", params.index);

				programmer.eraseBlockByNumber(params.index);

				INFO("Block %d erased.", params.index);
			};

			ret[op][EntryPoint::Mode::SECTOR] = [](Programmer &programmer, const EntryPoint::Parameters &params) {
				INFO("Erasing sector %d.", params.index);

				programmer.eraseSectorByNumber(params.index);

				INFO("Sector %d erased.", params.index);
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
				uint32_t address = params.index;

				const Flash &flashInfo = programmer.getFlashInfo();

				switch (params.mode) {
					case EntryPoint::Mode::BLOCK:  address *= flashInfo.getBlockSize();  break;
					case EntryPoint::Mode::SECTOR: address *= flashInfo.getSectorSize(); break;
					default:
						break;
				}

				{
					std::vector<uint8_t> page(flashInfo.getPageSize(), 0xff);

					while (! params.inStream->eof()) {
						params.inStream->read((char *) page.data(), page.size());

						size_t readSize = params.inStream->gcount();
						if (readSize == 0) {
							continue;
						}

						if (readSize != page.size()) {
							memset(page.data() + readSize, 0xff, page.size() - readSize);
						}

						programmer.writePage(address, page);

						address += page.size();
					}
				}
			};
		}

		op = EntryPoint::Operation::READ;
		{

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


int EntryPoint::call(Spi &spi, const FlashRegistry &registry, const Flash &flashGeometry, const Parameters &parameters) {
	return call(spi, registry, flashGeometry, std::vector<Parameters>({ parameters }));
}


int EntryPoint::call(Spi &spi, const FlashRegistry &registry, const Flash &flashGeometry, const std::vector<Parameters> &parameters) {
	int ret = RC_SUCCESS;

	try {
		Programmer programmer(spi, &registry);

		programmer.begin(flashGeometry.isGeometryValid() ? &flashGeometry : nullptr);
		{
			for (const auto &p : parameters) {
				if (p.mode != Mode::NONE && p.operation != Operation::NO_OPERATION) {
					auto handler = _getHandler(p);
					if (handler) {
						handler(programmer, p);
					}

				} else {
					// TODO:
				}
			}
		}
		programmer.end();

	} catch (const std::exception &ex) {
		ERROR("Got exception: %s", ex.what());

		ret = RC_FAILURE;
	}

	return ret;
}


bool EntryPoint::Parameters::isValid() const {
	ERROR("TODO: Implement!");
	return true;
}
