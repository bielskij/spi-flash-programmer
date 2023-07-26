/*
 * request.h
 *
 *  Created on: 23 lip 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef COMMON_PROTOCOL_REQUEST_H_
#define COMMON_PROTOCOL_REQUEST_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _ProtoReqGetInfo {

} ProtoReqGetInfo;


typedef struct _ProtoReqTransfer {
	uint8_t *txBuffer;
	uint16_t txBufferSize;

	uint8_t *rxBuffer;
	uint16_t rxBufferSize;

	uint16_t rxSkipSize;

	uint8_t flags;
} ProtoReqTransfer;


typedef struct _ProtoReq {
	uint8_t cmd;
	uint8_t id;

	union {
		ProtoReqGetInfo  getInfo;
		ProtoReqTransfer transfer;
	} request;
} ProtoReq;


void     proto_req_init  (ProtoReq *request, uint8_t cmd, uint8_t id);
void     proto_req_assign(ProtoReq *request, uint8_t *buffer, uint16_t bufferSize);
uint16_t proto_req_encode(ProtoReq *request, uint8_t *buffer, uint16_t bufferSize);
uint16_t proto_req_decode(ProtoReq *request, uint8_t *buffer, uint16_t bufferSize);

uint16_t proto_req_getPayloadSize(ProtoReq *request);

#ifdef __cplusplus
}
#endif

#endif /* COMMON_PROTOCOL_REQUEST_H_ */
