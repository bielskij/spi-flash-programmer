/*
 * common/protocol/packet.h
 *
 *  Created on: 23 lip 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef COMMON_PROTOCOL_PACKET_H_
#define COMMON_PROTOCOL_PACKET_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROTO_PKT_DES_RET_IDLE 0
#define PROTO_PKT_DES_RET_GET_ERROR_CODE(_v)((_v) & 0x7F)
#define PROTO_PKT_DES_RET_SET_ERROR_CODE(_v)((_v) | 0x80)

typedef struct _ProtoPkt {
	uint8_t    code;
	uint8_t    id;

	uint8_t   *payload;
	uint16_t   payloadSize;
} ProtoPkt;


typedef struct _ProtoPktDesCtx {
	uint8_t state;

	uint8_t id;
	uint8_t code;
	uint8_t crc;

	uint16_t dataSize;
	uint16_t dataRead;

	uint8_t *mem;
	uint16_t memSize;
} ProtoPktDes;


void     proto_pkt_init   (ProtoPkt *pkt, void *mem, uint16_t memSize, uint8_t code, uint8_t id);
bool     proto_pkt_prepare(ProtoPkt *pkt, void *mem, uint16_t memSize, uint16_t payloadSize);
uint16_t proto_pkt_encode (ProtoPkt *pkt, void *mem, uint16_t memSize);

void proto_pkt_dec_setup(ProtoPktDes *ctx, uint8_t *buffer, uint16_t bufferSize);

uint8_t proto_pkt_dec_putByte(ProtoPktDes *ctx, uint8_t byte, ProtoPkt *pkt);

void proto_pkt_dec_reset(ProtoPktDes *ctx);

#ifdef __cplusplus
}
#endif

#endif /* COMMON_PROTOCOL_PACKET_H_ */
