/*
 * flash/registry/reader.h
 *
 *  Created on: 09 nov 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef FLASHUTIL_FLASH_REGISTRY_READER_H_
#define FLASHUTIL_FLASH_REGISTRY_READER_H_

#include <iostream>

#include "flashutil/flash/registry.h"

class FlashRegistryReader {
	public:
		FlashRegistryReader() {
		}

		virtual ~FlashRegistryReader() {
		}

	public:
		virtual void read(std::istream &stream, FlashRegistry &registry) = 0;
};



#endif /* FLASHUTIL_FLASH_REGISTRY_READER_H_ */
