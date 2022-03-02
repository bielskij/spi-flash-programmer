#ifndef FLASHUTIL_INCLUDE_SERIAL_H_
#define FLASHUTIL_INCLUDE_SERIAL_H_

#include <stdbool.h>
#include <stdint.h>


typedef struct _Serial {
	bool (*write)(struct _Serial *self, void *buffer, size_t bufferSize, int timeoutMs);

	bool (*readByte)(struct _Serial *self, uint8_t *value, int timeoutMs);
} Serial;


Serial *new_serial(const char *serialPath);


#endif /* FLASHUTIL_INCLUDE_SERIAL_H_ */
