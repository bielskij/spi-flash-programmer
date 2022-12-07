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
			class ChipStatus {
				public:
					bool isProtected() const {
						return _protected;
					}

					void setProtected(bool isprotected) {
						_protected = isprotected;
					}

				private:
					bool _protected;
			};

		public:
			Programmer(SpiInterface &interface);

		public:
			bool connect();
			bool disconnect();

			Id   detectChip();

			void setSpec(const FlashSpec *spec);

		public:
			bool getChipStatus(ChipStatus &status);

		private:
			Programmer() = delete;
			Programmer(const Programmer &) = delete;
	};
}


#endif /* PROGRAMMER_H_ */
