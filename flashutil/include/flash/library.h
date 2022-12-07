/*
 * library.h
 *
 *  Created on: 6 gru 2022
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef FLASH_LIBRARY_H_
#define FLASH_LIBRARY_H_

#include "flash/spec.h"
#include "flash/id.h"

namespace flashutil {
	class FlashLibrary {
		public:
			static FlashLibrary &getInstance();

		public:
			void addFlashSpec(const FlashSpec &spec);

			const FlashSpec *getSpecById(const Id &id) const;

		private:
			FlashLibrary();
	};
}

#endif /* FLASH_LIBRARY_H_ */
