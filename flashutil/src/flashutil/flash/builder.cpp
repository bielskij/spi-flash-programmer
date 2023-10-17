/*
 * flash/builder.cpp
 *
 *  Created on: 11 wrz 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#include "flashutil/flash/builder.h"


FlashBuilder::FlashBuilder() {

}


FlashBuilder &FlashBuilder::setName(const std::string &name) {
	this->flash.setName(name);
	return *this;
}


FlashBuilder &FlashBuilder::setJedecId(const std::vector<uint8_t> &jedec) {
	this->flash.setId(jedec);
	return *this;
}


FlashBuilder &FlashBuilder::setBlockSize(std::size_t size) {
	this->flash.setBlockSize(size);
	return *this;
}


FlashBuilder &FlashBuilder::setBlockCount(std::size_t count) {
	this->flash.setBlockCount(count);
	return *this;
}


FlashBuilder &FlashBuilder::setSectorSize(std::size_t size) {
	this->flash.setSectorSize(size);
	return *this;
}


FlashBuilder &FlashBuilder::setSectorCount(std::size_t count) {
	this->flash.setSectorCount(count);
	return *this;
}


FlashBuilder &FlashBuilder::setPageSize(std::size_t size) {
	this->flash.setPageSize(size);
	return *this;
}


FlashBuilder &FlashBuilder::setPageCount(std::size_t count) {
	this->flash.setPageCount(count);
	return *this;
}


FlashBuilder &FlashBuilder::setSize(std::size_t size) {
	if (this->flash.getBlockSize()) {
		this->flash.setBlockCount(size / this->flash.getBlockSize());

	}

	if (this->flash.getSectorSize()) {
		this->flash.setSectorCount(size / this->flash.getSectorSize());
	}

	if (this->flash.getPageSize()) {
		this->flash.setPageCount(size / this->flash.getPageSize());
	}

	return *this;
}


FlashBuilder &FlashBuilder::setProtectMask(uint8_t mask) {
	this->setProtectMask(mask);
	return *this;
}


FlashBuilder &FlashBuilder::reset() {
	this->flash = Flash();
	return *this;
}


Flash FlashBuilder::build() {
	return this->flash;
}
