/*
 * flash/registry/reader/json.h
 *
 *  Created on: 09 nov 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef FLASHUTIL_FLASH_REGISTRY_READER_JSON_H_
#define FLASHUTIL_FLASH_REGISTRY_READER_JSON_H_

#include "flashutil/flash/registry/reader.h"


class FlashRegistryJsonReader : public FlashRegistryReader {
	public:
		FlashRegistryJsonReader();

		void read(std::istream &stream, FlashRegistry &registry) override;
};

#endif /* FLASHUTIL_FLASH_REGISTRY_READER_JSON_H_ */
