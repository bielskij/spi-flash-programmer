/*
 * programmer.h
 *
 *  Created on: 29 mar 2023
 *      Author: bielski.j@gmail.com
 */

#ifndef FIRMWARE_COMMON_PROTOCOL_H_
#define FIRMWARE_COMMON_PROTOCOL_H_

#include <stdint.h>
#include <stdbool.h>

typedef void (*SpiTransferCallback)(void *buffer, uint16_t txSize, uint16_t rxSize, void *callbackData);
typedef void (*SpiCsCallback)(bool assert, void *callbackData);

typedef void (*SerialSendCallback)(uint8_t data, void *callbackData);
typedef void (*SerialFlushCallback)(void *callbackData);


typedef struct _ProgrammerSetupParameters {
	// Memory used by programmer for:
	//  - holding internal structures
	//  - handling input data
	void    *memory;
	uint16_t memorySize;

	SpiTransferCallback spiTransferCallback;
	SpiCsCallback       spiCsCallback;

	SerialSendCallback  serialSendCallback;
	SerialFlushCallback serialFlushCallback;

	void *callbackData;
} ProgrammerSetupParameters;


bool programmer_setup(ProgrammerSetupParameters *parameters);

void programmer_onByte(uint8_t data);
void programmer_onData(uint8_t *data, uint16_t dataSize);
void programmer_onIdle();


#endif /* FIRMWARE_COMMON_PROTOCOL_H_ */
