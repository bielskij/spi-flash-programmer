#ifndef FLASHUTIL_INCLUDE_DEBUG_H_
#define FLASHUTIL_INCLUDE_DEBUG_H_

#include <stdint.h>
#include <ctype.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef enum _DebugLevel {
	DEBUG_LEVEL_NONE,

	DEBUG_LEVEL_ERROR,
	DEBUG_LEVEL_WARINNG,
	DEBUG_LEVEL_INFO,
	DEBUG_LEVEL_DEBUG,
	DEBUG_LEVEL_TRACE,

	DEBUG_LEVEL_LAST = DEBUG_LEVEL_TRACE
} DebugLevel;


#define ERROR(...) { debug_log(DEBUG_LEVEL_ERROR,   __FILE__, __LINE__, __func__, __VA_ARGS__); }
#define WARN(...)  { debug_log(DEBUG_LEVEL_WARINNG, __FILE__, __LINE__, __func__, __VA_ARGS__); }
#define INFO(...)  { debug_log(DEBUG_LEVEL_INFO,    __FILE__, __LINE__, __func__, __VA_ARGS__); }
#define DEBUG(...) { debug_log(DEBUG_LEVEL_DEBUG,   __FILE__, __LINE__, __func__, __VA_ARGS__); }
#define TRACE(...) { debug_log(DEBUG_LEVEL_TRACE,   __FILE__, __LINE__, __func__, __VA_ARGS__); }

#define OUT(...) { debug_out(__VA_ARGS__); }

#define HEX(level, title, buffer, bufferSize) do { \
	TRACE("Dumping memory at %p of size %zd bytes (%s)", buffer, bufferSize, title); \
	debug_dumpBuffer(level, buffer, bufferSize, 16, 0, 6); \
} while (0);

void debug_initialize();

void debug_setLevel(DebugLevel level);

void debug_log(DebugLevel level, const char *fileName, int lineNo, const char *functionName, const char *fmt, ...);
void debug_out(const char *fmt, ...);

void debug_dumpBuffer(DebugLevel level, uint8_t *buffer, uint32_t bufferSize, uint32_t lineLength, uint32_t offset, uint32_t indent);

#ifdef __cplusplus
}
#endif

#endif /* FLASHUTIL_INCLUDE_DEBUG_H_ */
