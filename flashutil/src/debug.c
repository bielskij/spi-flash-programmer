#include "debug.h"


void debug_dumpBuffer(uint8_t *buffer, uint32_t bufferSize, uint32_t lineLength, uint32_t offset) {
	uint32_t i;

	char *asciiBuffer = malloc(lineLength + 1);

	for (i = 0; i < bufferSize; i++) {
		if (i % lineLength == 0) {
			if (i != 0) {
				printf("  %s\n", asciiBuffer);
			}

			printf("%04x:  ", i + offset);
		}

		printf(" %02x", buffer[i]);

		if (! isprint(buffer[i])) {
			asciiBuffer[i % lineLength] = '.';
		} else {
			asciiBuffer[i % lineLength] = buffer[i];
		}

		asciiBuffer[(i % lineLength) + 1] = '\0';
	}

	while ((i % 16) != 0) {
		printf("   ");
		i++;
	}

	printf("  %s\n", asciiBuffer);

	free(asciiBuffer);
}
