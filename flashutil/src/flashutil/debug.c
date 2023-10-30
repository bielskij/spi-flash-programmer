#include <string.h>
#include <stdarg.h>

#include "flashutil/debug.h"

#define STRING_LEN_MAX 16

static DebugLevel _level = DEBUG_LEVEL_ERROR;


void debug_initialize() {

}


void debug_setLevel(DebugLevel level) {
	_level = level;
}


void debug_log(DebugLevel level, const char *fileName, int lineNo, const char *functionName, const char *fmt, ...) {
	char *levelString = "-----";
	char *levelColor  = NULL;

	switch (level) {
		case DEBUG_LEVEL_NONE:
			levelString = "NONE ";
			break;

		case DEBUG_LEVEL_ERROR:
			levelString = "ERROR";
			levelColor  = "\x1b[31m";
			break;

		case DEBUG_LEVEL_WARINNG:
			levelString = "WARN ";
			levelColor  = "\x1b[33m";
			break;

		case DEBUG_LEVEL_INFO:
			levelString = "INFO ";
			levelColor  = "\x1b[32m";
			break;

		case DEBUG_LEVEL_DEBUG:
			levelString = "DEBUG";
			levelColor  = "\x1b[34m";
			break;

		case DEBUG_LEVEL_TRACE:
			levelString = "TRACE";
			levelColor  = "\x1b[36m";
			break;
	}

	{
		size_t strLen;

		strLen = strlen(fileName);
		if (strLen > STRING_LEN_MAX) {
			fileName += (strLen - STRING_LEN_MAX);
		}

		strLen = strlen(functionName);
		if (strLen > STRING_LEN_MAX) {
			functionName += (strLen - STRING_LEN_MAX);
		}
	}

	printf("%s[%s] <%s:%5d> %s%16s(): ",
		levelColor != NULL ? levelColor : "",
		levelString,
		fileName,
		lineNo,
		levelColor != NULL ? "\x1b[0m" : "",
		functionName
	);

	{
		va_list ap;

		va_start(ap, fmt);
		{
			vprintf(fmt, ap);
		}
		va_end(ap);
	}

	printf("\n");
}


void debug_dumpBuffer(uint8_t *buffer, uint32_t bufferSize, uint32_t lineLength, uint32_t offset, uint32_t indent) {
	uint32_t i, j;

	char asciiBuffer[lineLength + 1];

	for (i = 0; i < bufferSize; i++) {
		if ((i % lineLength) == 0) {
			if (i != 0) {
				printf("  %s\n", asciiBuffer);
			}

			for (j = 0; j < indent; j++) {
				printf(" ");
			}

			printf("%04x:  ", i + offset);
		}

		printf(" %02x", buffer[i]);

		if (! isprint(buffer[i]) || buffer[i] == '\n' || buffer[i] == '\r') {
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
}
