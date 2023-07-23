/*
 * response.h
 *
 *  Created on: 23 lip 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef COMMON_PROTOCOL_RESPONSE_H_
#define COMMON_PROTOCOL_RESPONSE_H_

#include <stdint.h>


typedef struct _ProtocolResponseGetInfo {
	struct {
		uint8_t  major;
		uint8_t  minor;
		uint16_t payloadSize;
	} version;
} ProtocolResponseGetInfo;


typedef struct _ProtocolResponse {
	uint8_t id;

	union {
		ProtocolResponseGetInfo getInfo;
	} response;
} ProtocolResponse;

#endif /* COMMON_PROTOCOL_RESPONSE_H_ */
