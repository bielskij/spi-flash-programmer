/*
 * generic.h
 *
 *  Created on: 6 gru 2022
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef FLASH_OPERATIONS_GENERIC_H_
#define FLASH_OPERATIONS_GENERIC_H_

#include <memory>

#include "flash/operations.h"
#include "flash/spec.h"


namespace flashutil {
	class SpiInterface;

	class GenericOperations : public Operations {
		public:
			GenericOperations(const FlashSpec &spec, SpiInterface &iface);
			~GenericOperations();

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


#endif /* FLASH_OPERATIONS_GENERIC_H_ */
