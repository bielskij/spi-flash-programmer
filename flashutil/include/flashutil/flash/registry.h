/*
 * flashutil/flash/registry.h
 *
 *  Created on: 8 sep 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef FLASHUTIL_FLASH_REGISTRY_H_
#define FLASHUTIL_FLASH_REGISTRY_H_

#include "flashutil/flash.h"


class FlashRegistry {
	public:
		FlashRegistry();

		void clear();
		void addFlash(const Flash &flash);

		const Flash &getById(const std::vector<uint8_t> &id) const;
		const std::vector<Flash> &getAll() const;

	private:
		std::vector<Flash> _flashes;
};


#endif /* FLASHUTIL_FLASH_REGISTRY_H_ */
