/*
 * chip.h
 *
 *  Created on: 5 gru 2022
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef FLASH_CHIP_H_
#define FLASH_CHIP_H_

#include <set>
#include <memory>

#include "flash/id.h"
#include "flash/geometry.h"
#include "flash/opcode.h"
#include "flash/operations.h"

#define FLASHUTIL_SPEC_REGISTER() \
	static void specRegistration(void) __attribute__((constructor)); \
	void specRegistration(void)


namespace flashutil {
	class FlashSpec {
		public:
			FlashSpec(const std::string &name, const Id &id, const FlashGeometry &geometry, const std::set<FlashOpcode> &opcodes, std::unique_ptr<Operations> ops);

		public:
			const FlashGeometry &getFlashGeometry() const;
			FlashGeometry &getFlashGeometry();

			const Id &id() const;
			Id &id();

			bool hasOpcode(FlashOpcode opcode) const;

			const std::string &getName() const;

		private:
			std::string                 _name;
			Id                          _id;
			FlashGeometry               _geometry;
			std::set<FlashOpcode>       _opcodes;
			std::unique_ptr<Operations> _operations;
	};
}

#endif /* FLASH_CHIP_H_ */
