/*
 * main.c
 *
 *  Created on: 29 mar 2023
 *      Author: bielski.j@gmail.com
 */

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/spi.h"

#include "tusb.h"

#include "firmware/programmer.h"

#define CDC_CHANNEL 0


#define PROGRAMMER_MEMORY_POOL_SIZE 384

static uint8_t _programmerMemoryPool[PROGRAMMER_MEMORY_POOL_SIZE] = { 0 };


static void __not_in_flash_func(_spiTransferCallback)(void *buffer, uint16_t txSize, uint16_t rxSize, void *callbackData) {
	uint16_t loopSize = txSize > rxSize ? txSize : rxSize;

	for (uint16_t i = 0; i < loopSize; i++) {
		while (! spi_is_writable(spi_default)) {}

		if (i < txSize) {
			spi_get_hw(spi_default)->dr = ((uint8_t *)buffer)[i];

		} else {
			spi_get_hw(spi_default)->dr = 0xff;
		}

		while (! spi_is_readable(spi_default)) {}

		if (i < rxSize) {
			((uint8_t *)buffer)[i] = spi_get_hw(spi_default)->dr;

		} else {
			(void) spi_get_hw(spi_default)->dr;
		}
	}
}


static void _spiCsCallback(bool assert, void *callbackData) {
	asm volatile("nop \n nop \n nop"); // FIXME
	if (assert) {
		gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 0);

	} else {
		gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 1);
	}
	asm volatile("nop \n nop \n nop"); // FIXME
}


static void _serialSendCallback(uint8_t data, void *callbackData) {
	while (tud_cdc_n_write_char(CDC_CHANNEL, data) == 0) {
		tud_task();
	}
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

	spi_init(spi_default, 60 * 1000 * 1000);

	gpio_set_function(PICO_DEFAULT_SPI_RX_PIN , GPIO_FUNC_SPI);
	gpio_set_function(PICO_DEFAULT_SPI_TX_PIN,  GPIO_FUNC_SPI);
	gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);

	gpio_init(PICO_DEFAULT_SPI_CSN_PIN);
	gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 1);
	gpio_set_dir(PICO_DEFAULT_SPI_CSN_PIN, GPIO_OUT);

	{
		ProtocolReveicerSetupParameters params;

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
