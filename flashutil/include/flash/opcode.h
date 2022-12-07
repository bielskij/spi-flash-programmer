/*
 * opcode.h
 *
 *  Created on: 6 gru 2022
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef FLASH_OPCODE_H_
#define FLASH_OPCODE_H_

namespace flashutil {
	enum class FlashOpcode {
		H_01_WRITE_STATUS_REG,
		H_04_WRITE_DISABLE,
		H_05_READ_STATUS_REG,
		H_06_WRITE_ENABLE,
		H_9F_GET_JEDEC_ID
	};
}

#endif /* FLASH_OPCODE_H_ */
