/*
 * main.c
 *
 *  Created on: 29 mar 2023
 *      Author: bielski.j@gmail.com
 */

#include <stdio.h>

#include "hardware/spi.h"

#include "tusb.h"

#include "programmer.h"

#define CDC_CHANNEL 0


#define PROGRAMMER_MEMORY_POOL_SIZE 384

static uint8_t _programmerMemoryPool[PROGRAMMER_MEMORY_POOL_SIZE] = { 0 };


static void _spiTransferCallback(void *buffer, uint16_t txSize, uint16_t rxSize, void *callbackData) {

}


static void _spiCsCallback(bool assert, void *callbackData) {

}


static void _serialSendCallback(uint8_t data, void *callbackData) {
	tud_cdc_n_write_char(CDC_CHANNEL, data);
}


static void _serialFlushCallback(void *callbackData) {
	tud_cdc_n_write_flush(CDC_CHANNEL);
}


static void _cdcTask(void) {
	if (tud_cdc_n_available(CDC_CHANNEL)) {
		uint8_t buf[CFG_TUD_CDC_EP_BUFSIZE];

		uint32_t count = tud_cdc_n_read(CDC_CHANNEL, buf, sizeof(buf));

		programmer_onData(buf, count);
	}
}


int main() {
	board_init();
	tusb_init();

	{
		ProgrammerSetupParameters params;

		params.spiCsCallback       = _spiCsCallback;
		params.spiTransferCallback = _spiTransferCallback;
		params.serialFlushCallback = _serialFlushCallback;
		params.serialSendCallback  = _serialSendCallback;

		params.callbackData = NULL;

		params.memory     = _programmerMemoryPool;
		params.memorySize = PROGRAMMER_MEMORY_POOL_SIZE;

		programmer_setup(&params);
	}

	while (1) {
		tud_task();

		_cdcTask();
	}

	return 0;
}
