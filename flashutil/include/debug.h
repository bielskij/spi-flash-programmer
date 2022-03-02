#ifndef FLASHUTIL_INCLUDE_DEBUG_H_
#define FLASHUTIL_INCLUDE_DEBUG_H_

#include <stdint.h>
#include <ctype.h>
#include <stdio.h>

#define ERR(x) { printf("[ERR %s:%d]: ", __FUNCTION__, __LINE__); printf x; printf("\r\n"); }
#define DBG(x) { printf("[DBG %s:%d]: ", __FUNCTION__, __LINE__); printf x; printf("\r\n"); }
#define PRINTF(x) { printf x; printf("\n"); }

void debug_dumpBuffer(uint8_t *buffer, uint32_t bufferSize, uint32_t lineLength, uint32_t offset);

#endif /* FLASHUTIL_INCLUDE_DEBUG_H_ */
