#ifndef FLASHUTIL_ENTRYPOINT_H_
#define FLASHUTIL_ENTRYPOINT_H_

#include <iostream>

#include "flashutil/flash/registry.h"
#include "flashutil/flash.h"
#include "flashutil/spi.h"

namespace flashutil {
	class EntryPoint {
		public:
			static const int RC_SUCCESS;
			static const int RC_FAILURE;

		public:
			enum class Mode {
				NONE,

				CHIP,
				BLOCK,
				SECTOR
			};

			enum class Operation {
				NO_OPERATION,

				READ,
				WRITE,
				ERASE,
				UNLOCK
			};

			struct Parameters {
				int index;

				Mode      mode;
				Operation operation;

				bool omitRedundantWrites;
				bool verify;

				std::istream *inStream;
				std::ostream *outStream;

				Parameters() {
					this->index               = -1;
					this->mode                = Mode::NONE;
					this->operation           = Operation::NO_OPERATION;
					this->omitRedundantWrites = false;
					this->verify              = false;
					this->inStream            = nullptr;
					this->outStream           = nullptr;
				}

				bool isValid() const;
			};

		public:
			static int call(Spi &spi, const FlashRegistry &registry, const Flash &flashGeometry, const std::vector<Parameters> &parameters);
			static int call(Spi &spi, const FlashRegistry &registry, const Flash &flashGeometry, const Parameters &parameters);

		private:
			EntryPoint();
			EntryPoint(const EntryPoint &);
	};
}

#endif /* FLASHUTIL_ENTRYPOINT_H_ */
