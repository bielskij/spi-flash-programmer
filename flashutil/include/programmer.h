/*
 * Programmer.h
 *
 *  Created on: 5 gru 2022
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef PROGRAMMER_H_
#define PROGRAMMER_H_

#include "programmer/spi/interface.h"

namespace flashutil {
	class Programmer {
		public:
			Programmer(SpiInterface &interface);

		public:
			bool connect();
			bool disconnect();

		private:
			Programmer() = delete;
			Programmer(const Programmer &) = delete;
	};
}


#endif /* PROGRAMMER_H_ */
