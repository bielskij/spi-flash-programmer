/*
 * response.h
 *
 *  Created on: 23 lip 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef COMMON_PROTOCOL_RESPONSE_H_
#define COMMON_PROTOCOL_RESPONSE_H_

#include <stdint.h>
#include <stdbool.h>

#include "common/protocol/packet.h"

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


typedef struct _ProtoResTransfer {
	uint8_t *rxBuffer;
	uint16_t rxBufferSize;
} ProtoResTransfer;


typedef struct _ProtoRes {
	uint8_t cmd;
	uint8_t code;
	uint8_t id;

	union {
		ProtoResGetInfo  getInfo;
		ProtoResTransfer transfer;
	} response;
} ProtoRes;


void     proto_res_init  (ProtoRes *response, uint8_t cmd, uint8_t code, uint8_t id);
void     proto_res_assign(ProtoRes *response, uint8_t *buffer, uint16_t bufferSize);
uint16_t proto_res_encode(ProtoRes *response, uint8_t *buffer, uint16_t bufferSize);
uint16_t proto_res_decode(ProtoRes *response, uint8_t *buffer, uint16_t bufferSize);

uint16_t proto_res_getPayloadSize(ProtoRes *response);

#ifdef __cplusplus
}
#endif

#endif /* COMMON_PROTOCOL_RESPONSE_H_ */
