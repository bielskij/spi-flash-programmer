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
			class StatusReg {
				public:
					StatusReg(uint32_t busyMask, uint32_t writeProtectedMask, uint32_t writeEnabledMask, uint32_t protectionBitsMask);
					virtual ~StatusReg();

				public:
					void load(uint32_t val) const;
					uint32_t getRaw() const ;

					virtual bool isBusy() const;
					virtual bool isWriteProtected() const;
					virtual bool isWriteEnabled() const;
					virtual uint32_t getProtectionBits() const;

					virtual void setWriteProtected(bool protect);
					virtual void setProtectionBits(uint32_t bits);
			};

		public:
			FlashSpec(
				const std::string &name,
				const Id &id,
				const FlashGeometry &geometry,
				const std::set<FlashOpcode> &opcodes,
				std::unique_ptr<Operations> customOps
			);

		public:
			const FlashGeometry   &getFlashGeometry() const;
			FlashGeometry         &getFlashGeometry();

			const Id &id() const;
			Id       &id();

			bool hasOpcode(FlashOpcode opcode) const;

			const std::string &getName() const;

			const StatusReg *getStatusReg(uint32_t regValue);

		private:
			std::string                 _name;
			Id                          _id;
			FlashGeometry               _geometry;
			std::set<FlashOpcode>       _opcodes;
			std::unique_ptr<Operations> _operations;
			std::unique_ptr<StatusReg>  _statusReg;
	};
}

#endif /* FLASH_CHIP_H_ */
