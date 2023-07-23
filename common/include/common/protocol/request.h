/*
 * request.h
 *
 *  Created on: 23 lip 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef COMMON_PROTOCOL_REQUEST_H_
#define COMMON_PROTOCOL_REQUEST_H_

#include <stdint.h>

typedef struct _PRQGetInfo {

} PRQGetInfo;


typedef struct _PRQDError {
	uint8_t code;
} PRQDError;


typedef struct _PRQ {
	uint8_t cmd;
	uint8_t id;

	union {
		PRQGetInfo getInfo;
		PRQDError  error;
	} request;
} PRQ;


#endif /* COMMON_PROTOCOL_REQUEST_H_ */
