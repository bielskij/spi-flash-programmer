/*
 * request.h
 *
 *  Created on: 23 lip 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef COMMON_PROTOCOL_REQUEST_H_
#define COMMON_PROTOCOL_REQUEST_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _ProtoReqGetInfo {

} ProtoReqGetInfo;


typedef struct _ProtoReq {
	uint8_t cmd;
	uint8_t id;

	union {
		ProtoReqGetInfo getInfo;
	} request;
} ProtoReq;

#ifdef __cplusplus
}
#endif

#endif /* COMMON_PROTOCOL_REQUEST_H_ */
