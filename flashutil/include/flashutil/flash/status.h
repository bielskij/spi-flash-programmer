/*
 * status.h
 *
 *  Created on: 1 lis 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef FLASHUTIL_FLASH_STATUS_H_
#define FLASHUTIL_FLASH_STATUS_H_

#include <cstdint>

class FlashStatus {
	public:
		FlashStatus(uint8_t status);

		bool isWriteEnableLatchSet() const;
		void setWriteEnableLatch(bool set);

		bool isWriteInProgress() const;
		void setBusy(bool busy);

		uint8_t getRegisterValue() const;

	private:
		uint8_t _reg;
};

#endif /* FLASHUTIL_FLASH_STATUS_H_ */
