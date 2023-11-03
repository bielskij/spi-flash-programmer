/*
 * main.c
 *
 *  Created on: 11 wrz 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#include "flashutil/flash.h"


Flash::Flash() {
	this->blockSize   = 0;
	this->blockCount  = 0;
	this->sectorSize  = 0;
	this->sectorCount = 0;
	this->pageSize    = 0;
	this->pageCount   = 0;
	this->protectMask = 0;
}


Flash::Flash(const std::string &name, const std::vector<uint8_t> &jedecId, size_t blockSize, size_t nblocks, size_t sectorSize, size_t nSectors, uint8_t protectMask) {
	this->setName(name);
	this->setId(jedecId);
	this->setBlockSize(blockSize);
	this->setBlockCount(nblocks);
	this->setSectorSize(sectorSize);
	this->setSectorCount(nSectors);
	this->setProtectMask(protectMask);

	this->setPageSize(256);
	this->setPageCount(this->getSize() / this->getPageSize());
}


const std::string &Flash::getName() const {
	return this->name;
}


void Flash::setName(const std::string &name) {
	this->name = name;
}


const std::vector<uint8_t> &Flash::getId() const {
	return this->id;
}


void Flash::setId(const std::vector<uint8_t> &id) {
	this->id = id;
}


size_t Flash::getBlockSize() const {
	return this->blockSize;
}


void Flash::setBlockSize(size_t size) {
	this->blockSize = size;
}


size_t Flash::getBlockCount() const {
	return this->blockCount;
}


void Flash::setBlockCount(size_t count) {
	this->blockCount = count;
}


size_t Flash::getSectorSize() const {
	return this->sectorSize;
}


void Flash::setSectorSize(size_t size) {
	this->sectorSize = size;
}


size_t Flash::getSectorCount() const {
	return this->sectorCount;
}


void Flash::setSectorCount(size_t count) {
	this->sectorCount = count;
}


size_t Flash::getPageSize() const {
	return this->pageSize;
}


void Flash::setPageSize(size_t size) {
	this->pageSize = size;
}


size_t Flash::getPageCount() const {
	return this->pageCount;
}


void Flash::setPageCount(size_t count) {
	this->pageCount = count;
}


uint8_t Flash::getProtectMask() const {
	return this->protectMask;
}


void Flash::setProtectMask(uint8_t mask) {
	this->protectMask = mask;
}


size_t Flash::getSize() const {
	if (this->blockCount != 0 && this->blockSize != 0) {
		return this->blockCount * this->blockSize;

	} else if (this->sectorCount != 0 && this->sectorSize != 0) {
		return this->sectorCount * this->sectorSize;
	}

	return 0;
}


void Flash::setGeometry(const Flash &other) {
	this->setBlockCount(other.getBlockCount());
	this->setBlockSize (other.getBlockSize());

	this->setSectorCount(other.getSectorCount());
	this->setSectorSize (other.getSectorSize());

	this->setPageCount(other.getPageCount());
	this->setPageSize (other.getPageSize());

	this->setProtectMask(other.getProtectMask());
}


bool Flash::isIdValid() const {
	for (auto b : this->id) {
		if (b != 0x00 && b != 0xff) {
			return true;
		}
	}

	return false;
}


bool Flash::isGeometryValid() const {
	if (
		this->getBlockCount()  == 0 ||
		this->getBlockSize()   == 0 ||
		this->getSectorCount() == 0 ||
		this->getSectorSize()  == 0
	) {
		return false;
	}

	return true;
}


bool Flash::isValid() const {
	if (! this->isIdValid()) {
		return false;
	}

	return this->isGeometryValid();
}
