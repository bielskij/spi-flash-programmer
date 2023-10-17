/*
 * flash/registry.cpp
 *
 *  Created on: 11 wrz 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#include <stdexcept>

#include "flashutil/flash/registry.h"
#include "flashutil/flash/builder.h"


FlashRegistry::FlashRegistry() {

}


void FlashRegistry::addFlash(const Flash &flash) {
	this->_flashes.push_back(flash);
}


const Flash &FlashRegistry::getById(const std::vector<uint8_t> &id) const {
	for (const auto &f : this->_flashes) {
		if (f.getId() == id) {
			return f;
		}
	}

	throw std::runtime_error("Flash info was not found in local repository!");
}
