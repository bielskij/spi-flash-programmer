#ifndef COMMON_INCLUDE_CRC8_H_
#define COMMON_INCLUDE_CRC8_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

uint8_t crc8_getForByte(uint8_t byte, uint8_t polynomial, uint8_t start);

uint8_t crc8_get(uint8_t *buffer, uint16_t bufferSize, uint8_t polynomial, uint8_t start);

#ifdef __cplusplus
}
#endif

#endif /* COMMON_INCLUDE_CRC8_H_ */
