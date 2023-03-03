#ifndef FLASHUTIL_INCLUDE_SERIAL_H_
#define FLASHUTIL_INCLUDE_SERIAL_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _Serial {
	bool (*write)(struct _Serial *self, void *buffer, size_t bufferSize, int timeoutMs);

	bool (*readByte)(struct _Serial *self, uint8_t *value, int timeoutMs);
} Serial;


Serial *new_serial(const char *serialPath, int baud);

void free_serial(Serial *serial);

#ifdef __cplusplus
}
#endif

#endif /* FLASHUTIL_INCLUDE_SERIAL_H_ */
