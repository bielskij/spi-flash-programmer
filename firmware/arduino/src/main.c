/*
 * main.c
 *
 *  Created on: 27 lut 2022
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */


#include <avr/io.h>

#include "common/protocol.h"

#include "firmware/programmer.h"

#ifndef NULL
	#define NULL ((void *) 0)
#endif

#define CONCAT_MACRO(port, letter) port ## letter
#define DECLARE_PORT(port) CONCAT_MACRO(PORT, port)
#define DECLARE_DDR(port)  CONCAT_MACRO(DDR,  port)
#define DECLARE_PIN(port)  CONCAT_MACRO(PIN,  port)

#define PIO_SET_HIGH(bank, pin) { DECLARE_PORT(bank) |= _BV(DECLARE_PIN(pin)); }
#define PIO_SET_LOW(bank, pin)  { DECLARE_PORT(bank) &= ~_BV(DECLARE_PIN(pin)); }

#define PIO_TOGGLE(bank, pin) { DECLARE_PORT(bank) ^= _BV(DECLARE_PIN(pin)); }

#define PIO_IS_HIGH(bank, pin) (bit_is_set(DECLARE_PIN(bank), DECLARE_PIN(pin)))
#define PIO_IS_LOW(bank, pin) (! bit_is_set(DECLARE_PIN(bank), DECLARE_PIN(pin)))

#define PIO_SET_OUTPUT(bank, pin) { DECLARE_DDR(bank) |= _BV(DECLARE_PIN(pin)); }
#define PIO_SET_INPUT(bank, pin)  { DECLARE_DDR(bank) &= ~(_BV(DECLARE_PIN(pin))); }

/************************
 * UART
 */

#if BAUD >= 1000000
	#if F_CPU == 8000000
		#define F_CPU_PRESCALER 8
	#else
		#define F_CPU_PRESCALER 16
	#endif
#else
		#define F_CPU_PRESCALER 16
#endif


#define UART_BAUD_REG (((F_CPU / F_CPU_PRESCALER) / BAUD) - 1)

#define _waitForTransmit() while (! (UCSR0A & _BV(UDRE0)));

void uart_initialize() {
	// Configure usart
	UBRR0H = ((UART_BAUD_REG) >> 8);
	UBRR0L = ((UART_BAUD_REG) & 0x00FF);

#if F_CPU_PRESCALER == 8
	UCSR0A |= _BV(U2X0);
#endif

	// 8bit, 1bit stop, no parity
	UCSR0C  = _BV(UCSZ00) | _BV(UCSZ01);

	// enable
	UCSR0B |= (_BV(TXEN0) | _BV(RXEN0));
}


void uart_send(char c) {
	_waitForTransmit();

	UDR0 = c;
}


char uart_poll() {
	if (UCSR0A & _BV(RXC0)) {
		return 1;
	}

	return 0;
}


uint8_t uart_get() {
	return UDR0;
}


/************************
 * SPI
 */

#define SPI_CS_BANK B
#define SPI_CS_PIN  2

#define CS_LOW()    PIO_SET_LOW(SPI_CS_BANK, SPI_CS_PIN)
#define CS_HIGH()   PIO_SET_HIGH(SPI_CS_BANK, SPI_CS_PIN)

#define SPI_WAIT() { while (! (SPSR & _BV(SPIF))) {} }

#define _max(x, y) ((x) < (y) ? (y) : (x))

void spi_initialize() {
	PIO_SET_OUTPUT(SPI_CS_BANK, SPI_CS_PIN);
	CS_HIGH();

	PIO_SET_OUTPUT(B, 3); // MOSI
	PIO_SET_OUTPUT(B, 5); // SCK

	// clear double speed
	SPSR &= ~_BV(SPI2X);

	// Enable SPI, Master, clock rate f_osc/2, MODE 0
	SPSR = _BV(SPI2X);
	SPCR = _BV(SPE) | _BV(MSTR);
}


static void _programmerRequestCallback(ProtoReq *request, ProtoRes *response, void *callbackData) {
	switch (request->cmd) {
		case PROTO_CMD_SPI_TRANSFER:
			{
				ProtoReqTransfer *req = &request->request.transfer;
				ProtoResTransfer *res = &response->response.transfer;

				CS_LOW();
				{
					uint16_t toRecv = req->rxBufferSize;

					for (uint16_t i = 0; i < _max(req->txBufferSize, req->rxSkipSize + req->rxBufferSize); i++) {
						if (i < req->txBufferSize) {
							SPDR = req->txBuffer[i];

						} else {
							SPDR = 0xff;
						}

						SPI_WAIT();

						if (toRecv) {
							if (i >= req->rxSkipSize) {
								res->rxBuffer[i - req->rxSkipSize] = SPDR;

								toRecv--;
							}
						}
					}
				}
				if ((req->flags & PROTO_SPI_TRANSFER_FLAG_KEEP_CS) == 0) {
					CS_HIGH();
				}
			}
			break;

		default:
			break;
	}
}


static void _programmerResponseCallback(uint8_t *buffer, uint16_t bufferSize, void *callbackData) {
	for (uint16_t i = 0; i < bufferSize; i++) {
		uart_send(buffer[i]);
	}
}


#define DATA_BUFFER_SIZE 512

static uint8_t _dataBuffer[DATA_BUFFER_SIZE];


int main(int argc, char *argv[]) {
	Programmer programmer;

	uart_initialize();
	spi_initialize();

	programmer_setup(
		&programmer,
		_dataBuffer,
		DATA_BUFFER_SIZE,
		_programmerRequestCallback,
		_programmerResponseCallback,
		NULL
	);

	{
		uint16_t idleCounter = 0;

		while (1) {
			if (! uart_poll()) {
				if (++idleCounter == 60000) {
					programmer_reset(&programmer);
				}

			} else {
				idleCounter = 0;

				programmer_putByte(&programmer, uart_get());
			}
		}
	}
}
