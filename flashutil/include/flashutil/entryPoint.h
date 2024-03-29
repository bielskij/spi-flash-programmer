/*
 * flashutil/entryPoint.h
 *
 *  Created on: 17 oct 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef FLASHUTIL_ENTRYPOINT_H_
#define FLASHUTIL_ENTRYPOINT_H_

#include <iostream>

#include "flashutil/flash/registry.h"
#include "flashutil/flash.h"
#include "flashutil/spi.h"

namespace flashutil {
	class EntryPoint {
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

				Flash        *flashInfo;
				std::istream *inStream;
				std::ostream *outStream;

				std::function<void(const Parameters &)> beforeExecution;
				std::function<void(const Parameters &)> afterExecution;

				Parameters() {
					this->index               = 0;
					this->mode                = Mode::NONE;
					this->operation           = Operation::NO_OPERATION;
					this->omitRedundantWrites = false;
					this->verify              = false;
					this->inStream            = nullptr;
					this->outStream           = nullptr;
					this->flashInfo           = nullptr;
				}

				bool isValid() const;
			};

		public:
			static void call(Spi &spi, const FlashRegistry &registry, const Flash &flashGeometry, const std::vector<Parameters> &parameters);
			static void call(Spi &spi, const FlashRegistry &registry, const Flash &flashGeometry, const Parameters &parameters);

		private:
			EntryPoint();
			EntryPoint(const EntryPoint &);
	};
}

#endif /* FLASHUTIL_ENTRYPOINT_H_ */
