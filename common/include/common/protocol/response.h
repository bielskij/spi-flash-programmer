/*
 * response.h
 *
 *  Created on: 23 lip 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef COMMON_PROTOCOL_RESPONSE_H_
#define COMMON_PROTOCOL_RESPONSE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _ProtoResError {
	uint8_t code;
} ProtoResError;


typedef struct _ProtoResGetInfo {
	struct {
		uint8_t  major;
		uint8_t  minor;
	} version;

	uint16_t payloadSize;
} ProtoResGetInfo;


typedef struct _ProtoRes {
	uint8_t cmd;
	uint8_t id;

	union {
		ProtoResError   error;
		ProtoResGetInfo getInfo;
	} response;
} ProtoRes;

#ifdef __cplusplus
}
#endif

#endif /* COMMON_PROTOCOL_RESPONSE_H_ */
