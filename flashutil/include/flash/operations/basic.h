/*
 * basic.h
 *
 *  Created on: 6 gru 2022
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef FLASH_OPERATIONS_BASIC_H_
#define FLASH_OPERATIONS_BASIC_H_

#include <memory>

#include "flash/operations.h"
#include "flash/spec.h"
#include "flash/opcode.h"


namespace flashutil {
	class SpiInterface;

	class BasicOperations : public Operations {
		public:
			BasicOperations();
			~BasicOperations();

			void setSpec(const FlashSpec *spec);
			void setSpi(SpiInterface *spi);

			static const std::set<FlashOpcode> &getOpcodes();

		public:
			virtual bool getChipId(Id &id) override;

			virtual bool erase() override;
			virtual bool eraseBlock() override;
			virtual bool eraseSector() override;

		private:
			class Impl;

			std::unique_ptr<Impl> self;
	};
}

#endif /* FLASH_OPERATIONS_BASIC_H_ */
