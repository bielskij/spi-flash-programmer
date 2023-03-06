#ifndef FLASHUTIL_INCLUDE_DEBUG_H_
#define FLASHUTIL_INCLUDE_DEBUG_H_

#include <stdint.h>
#include <ctype.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(DEBUG)
	#define DEBUG 0
#endif

#if DEBUG == 1
	#define ERR(x) do { printf("[ERR %s:%d]: ", __FUNCTION__, __LINE__); printf x; printf("\r\n"); } while (0)
#else
	#define ERR(x) do { } while (0)
#endif

#if DEBUG == 1
	#define DBG(x) do { printf("[DBG %s:%d]: ", __FUNCTION__, __LINE__); printf x; printf("\r\n"); } while (0)
#else
	#define DBG(x) do { } while (0)
#endif

#define PRINTF(x) { printf x; printf("\n"); }

void debug_dumpBuffer(uint8_t *buffer, uint32_t bufferSize, uint32_t lineLength, uint32_t offset);

#ifdef __cplusplus
}
#endif

#endif /* FLASHUTIL_INCLUDE_DEBUG_H_ */
