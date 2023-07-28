#include "common/protocol/request.h"
#include "common/protocol/response.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ProgrammerRequestCallback)(ProtoReq *request, ProtoRes *response, void *callbackData);
typedef void (*ProgrammerResponseCallback)(uint8_t *buffer, uint16_t bufferSize, void *callbackData);

typedef struct _Programmer {
	uint8_t *mem;
	uint16_t memSize;

	ProtoPktDes packetDeserializer;

	ProgrammerRequestCallback  requestCallback;
	ProgrammerResponseCallback responseCallback;
	void                      *callbackData;
} Programmer;


void programmer_setup(
	Programmer                *programmer,
	uint8_t                   *memory,
	uint16_t                   memorySize,
	ProgrammerRequestCallback  requestCallback,
	ProgrammerResponseCallback responseCallback,
	void                      *callbackData
);

void programmer_putByte(Programmer *programmer, uint8_t byte);

void programmer_reset(Programmer *programmer);

#ifdef __cplusplus
}
#endif
