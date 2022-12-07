/*
 * operation.h
 *
 *  Created on: 6 gru 2022
 *      Author: jarko
 */

#ifndef FLASH_OPERATION_H_
#define FLASH_OPERATION_H_

#include "flash/id.h"

namespace flashutil {
	class Operations {
		public:
			class OperationNotSupportedException : public std::runtime_error {
				public:
					OperationNotSupportedException() : std::runtime_error("Operation is not supported by the chip!") {
					}
			};

			class OperationNotImplementedException : public std::runtime_error {
				public:
					OperationNotImplementedException() : std::runtime_error("Operation is supported by the chip, but not implemented yet!") {
					}
			};

		public:
			virtual ~Operations() {}

		public:
			virtual bool getChipId(Id &id) = 0;

			virtual bool erase() = 0;
			virtual bool eraseBlock() = 0;
			virtual bool eraseSector() = 0;
	};
}


#endif /* FLASH_OPERATION_H_ */
