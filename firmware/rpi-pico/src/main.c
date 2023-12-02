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

#include "common/protocol.h"
#include "firmware/programmer.h"

#define _max(x, y) ((x) < (y) ? (y) : (x))

#define CDC_CHANNEL 0

#define PROGRAMMER_MEMORY_POOL_SIZE 384

static uint8_t _programmerMemoryPool[PROGRAMMER_MEMORY_POOL_SIZE] = { 0 };
static Programmer programmer;

static void _spiCsCallback(bool assert) {
	asm volatile("nop \n nop \n nop"); // FIXME
	if (assert) {
		gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 0);

	} else {
		gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 1);
	}
	asm volatile("nop \n nop \n nop"); // FIXME
}


static void _programmerResponseCallback(uint8_t *buffer, uint16_t bufferSize, void *callbackData) {
	for (uint16_t i = 0; i < bufferSize; i++) {
		while (tud_cdc_n_write_char(CDC_CHANNEL, buffer[i]) == 0) {
			tud_task();
		}
	}

	tud_cdc_n_write_flush(CDC_CHANNEL);
}


static void _programmerRequestCallback(ProtoReq *request, ProtoRes *response, void *callbackData) {
	switch (request->cmd) {
		case PROTO_CMD_SPI_TRANSFER:
			{
				ProtoReqTransfer *req = &request->request.transfer;
				ProtoResTransfer *res = &response->response.transfer;

				_spiCsCallback(true);
				{
					uint16_t toRecv = req->rxBufferSize;

					for (uint16_t i = 0; i < _max(req->txBufferSize, req->rxSkipSize + req->rxBufferSize); i++) {
						while (! spi_is_writable(spi_default)) {}

						if (i < req->txBufferSize) {
							spi_get_hw(spi_default)->dr = req->txBuffer[i];

						} else {
							spi_get_hw(spi_default)->dr = 0xff;
						}

						while (! spi_is_readable(spi_default)) {}

						if (toRecv) {
							if (i >= req->rxSkipSize) {
								res->rxBuffer[i - req->rxSkipSize] = spi_get_hw(spi_default)->dr;

								toRecv--;

							} else {
								(void) spi_get_hw(spi_default)->dr;
							}

						} else {
							(void) spi_get_hw(spi_default)->dr;
						}
					}
				}
				if ((req->flags & PROTO_SPI_TRANSFER_FLAG_KEEP_CS) == 0) {
					_spiCsCallback(false);
				}
			}
			break;

		default:
			break;
	}
}


static void _cdcTask(void) {
	if (tud_cdc_n_available(CDC_CHANNEL)) {
		uint8_t buf[CFG_TUD_CDC_EP_BUFSIZE];

		uint32_t count = tud_cdc_n_read(CDC_CHANNEL, buf, sizeof(buf));

		for (uint32_t i = 0; i < count; i++) {
			programmer_putByte(&programmer, buf[i]);
		}
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

	programmer_setup(
		&programmer,
		_programmerMemoryPool,
		PROGRAMMER_MEMORY_POOL_SIZE,
		_programmerRequestCallback,
		_programmerResponseCallback,
		NULL
	);

	while (1) {
		tud_task();

		_cdcTask();
	}

	return 0;
}
