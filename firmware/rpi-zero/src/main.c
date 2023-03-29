/*
 * main.c
 *
 *  Created on: 29 mar 2023
 *      Author: bielski.j@gmail.com
 */

#include <stdio.h>

#include "hardware/spi.h"

#include "tusb.h"

#define CDC_CHANNEL 0

static void _cdcTask(void) {
	if (tud_cdc_n_available(CDC_CHANNEL)) {
		uint8_t buf[CFG_TUD_CDC_EP_BUFSIZE];

		uint32_t count = tud_cdc_n_read(CDC_CHANNEL, buf, sizeof(buf));

//		tud_cdc_n_write_str(CDC_CHANNEL, "A!\r\n");
//		tud_cdc_n_write(CDC_CHANNEL, buf, count);
//		tud_cdc_n_write_flush(CDC_CHANNEL);
	}
}


int main() {
	board_init();
	tusb_init();

	while (1) {
		tud_task();

		_cdcTask();
	}

	return 0;
}
