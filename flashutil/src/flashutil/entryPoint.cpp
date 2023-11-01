#include <functional>
#include <map>

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
				programmer.eraseChip();
			};

			ret[op][EntryPoint::Mode::BLOCK] = [](Programmer &programmer, const EntryPoint::Parameters &params) {
				programmer.eraseBlockByNumber(params.index, params.omitRedundantWrites);
			};

			ret[op][EntryPoint::Mode::SECTOR] = [](Programmer &programmer, const EntryPoint::Parameters &params) {
				programmer.eraseSectorByNumber(params.index, params.omitRedundantWrites);
			};
		}

		op = EntryPoint::Operation::UNLOCK;
		{
			ret[op][EntryPoint::Mode::CHIP] = [](Programmer &programmer, const EntryPoint::Parameters &params) {
				programmer.unlockChip();
			};
		}

		op = EntryPoint::Operation::WRITE;
		{

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
