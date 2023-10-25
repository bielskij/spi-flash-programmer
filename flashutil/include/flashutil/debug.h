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

#define _printErr(...) { fprintf(stderr, __VA_ARGS__); }

#if DEBUG == 1
	#define ERR(x) do { _printErr("[ERR %s:%d]: ", __FUNCTION__, __LINE__); _printErr x; _printErr("\r\n"); } while (0)
#else
	#define ERR(x) do { } while (0)
#endif

#if DEBUG == 1
	#define DBG(x) do { _printErr("[DBG %s:%d]: ", __FUNCTION__, __LINE__); _printErr x; _printErr("\r\n"); } while (0)
#else
	#define DBG(x) do { } while (0)
#endif

#if DEBUG == 1
	#define TRACE(x) do { _printErr("[TRC %s:%d]: ", __FUNCTION__, __LINE__); _printErr x; _printErr("\r\n"); } while (0)
#else
	#define TRACE(x) do { } while (0)
#endif

#define PRINTF(x) { _printErr x  }
#define PRINTFLN(x) { PRINTF(x); PRINTF(("\n")); }

void debug_dumpBuffer(uint8_t *buffer, uint32_t bufferSize, uint32_t lineLength, uint32_t offset);

#ifdef __cplusplus
}
#endif

#endif /* FLASHUTIL_INCLUDE_DEBUG_H_ */
