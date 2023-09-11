/*
 * flash/registry.h
 *
 *  Created on: 8 wrz 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef FLASH_REGISTRY_H_
#define FLASH_REGISTRY_H_

#include "flash.h"


class FlashRegistry {
	public:
		FlashRegistry();

		void addFlash(const Flash &flash);

		const Flash &getById(const std::vector<uint8_t> &id) const;

	private:
		std::vector<Flash> _flashes;
};


#endif /* FLASH_REGISTRY_H_ */
