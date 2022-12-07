/*
 * Interface.h
 *
 *  Created on: 5 gru 2022
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef INTERFACE_H_
#define INTERFACE_H_

#include "SpiInterface.h"

namespace flashutil {
	class SpiFlashInterface {
		public:
			SpiFlashInterface(SpiInterface &spi);

			virtual ~SpiFlashInterface() {}

		public:
			virtual bool writeEnable(bool enable);

		private:
			struct Impl;

			std::unique_ptr<Impl> self;
	};
}

#endif /* INTERFACE_H_ */
