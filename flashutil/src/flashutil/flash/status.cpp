/*
 * status.cpp
 *
 *  Created on: 1 lis 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#include "flashutil/flash/status.h"

// TODO: Each chip should define mask for those flags
#define STATUS_FLAG_WLE 0x02
#define STATUS_FLAG_WIP 0x01


FlashStatus::FlashStatus() : FlashStatus(0) {
}


FlashStatus::FlashStatus(uint8_t status) {
	this->_reg = status;
}

bool FlashStatus::isWriteEnableLatchSet() const {
	return (this->_reg & STATUS_FLAG_WLE) != 0;
}

void FlashStatus::setWriteEnableLatch(bool set) {
	this->_reg |= STATUS_FLAG_WLE;
}

bool FlashStatus::isWriteInProgress() const {
	return (this->_reg & STATUS_FLAG_WIP) != 0;
}

void FlashStatus::setBusy(bool busy) {
	this->_reg |= STATUS_FLAG_WIP;
}

uint8_t FlashStatus::getRegisterValue() const {
	return this->_reg;
}


void FlashStatus::setRegisterValue(uint8_t value) {
	this->_reg = value;
}


bool FlashStatus::isProtected(uint8_t mask) const {
	return (this->_reg & mask) != 0;
}


void FlashStatus::unprotect(uint8_t mask) {
	this->_reg &= ~mask;
}
