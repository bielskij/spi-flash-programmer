/*
 * request.h
 *
 *  Created on: 23 lip 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef COMMON_PROTOCOL_DESERIALIZER_REQUEST_H_
#define COMMON_PROTOCOL_DESERIALIZER_REQUEST_H_

#include <stdbool.h>

#include "common/protocol/request.h"

#define PRQD_RET_IDLE 0
#define PRQD_RET_GET_ERROR_CODE(_v)((_v) & 0x7F)
#define PRQD_RET_SET_ERROR_CODE(_v)((_v) | 0x80)

// PRQD = Protocol Request Deserializer

typedef struct _PRQDContext {
	uint8_t state;

	uint8_t id;
	uint8_t cmd;
	uint8_t crc;

	uint16_t dataSize;
	uint16_t dataRead;

	uint8_t *mem;
	uint16_t memSize;
} PRQDContext;


void PRQD_setup(PRQDContext *ctx, void *mem, uint16_t memSize);

bool PRQD_putByte(PRQDContext *ctx, uint8_t byte, PRQ *request);

void PRQD_reset(PRQDContext *ctx);

#endif /* COMMON_PROTOCOL_DESERIALIZER_REQUEST_H_ */
