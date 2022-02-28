/*
 * main.c
 *
 *  Created on: 27 lut 2022
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */


#include <avr/io.h>

#include "protocol.h"

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

#define UART_BAUD 38400
#define UART_BAUD_REG (((F_CPU / 16) / UART_BAUD) - 1)

#define _waitForTransmit() while (! (UCSR0A & _BV(UDRE0)));

void uart_initialize() {
	// Configure usart
	UBRR0H = ((UART_BAUD_REG) >> 8);
	UBRR0L = ((UART_BAUD_REG) & 0x00FF);

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


void spi_transfer(uint8_t *buffer, uint16_t txSize, uint16_t rxSize) {
	uint16_t loopSize = txSize > rxSize ? txSize : rxSize;

	for (uint16_t i = 0; i < loopSize; i++) {
		if (i < txSize) {
			SPDR = buffer[i];

		} else {
			SPDR = 0xff;
		}

		SPI_WAIT();

		if (i < rxSize) {
			buffer[i] = SPDR;
		}
	}
}


static uint8_t crc8_getForByte(uint8_t byte, uint8_t polynomial, uint8_t start) {
	uint8_t remainder = start;

	remainder ^= byte;

	{
		uint8_t bit;

		for (bit = 0; bit < 8; bit++) {
			if (remainder & 0x01) {
				remainder = (remainder >> 1) ^ polynomial;

			} else {
				remainder = (remainder >> 1);
			}

		}
	}

	return remainder;
}



#define SPI_BUFFER_SIZE 256

static uint8_t  _spiBuffer[SPI_BUFFER_SIZE];
static uint16_t _spiBufferWritten;


typedef enum _RxState {
	RX_STATE_WAIT_SYNC,
	RX_STATE_WAIT_CMD,

	RX_STATE_WAIT_DATA_LEN_HI,
	RX_STATE_WAIT_DATA_LEN_LO,

	RX_STATE_WAIT_DATA,

	RX_STATE_WAIT_CRC
} RxState;


static void _response(uint8_t code, uint8_t *buffer, uint16_t bufferSize) {
	uint8_t crc = 0;

	uart_send(PROTO_SYNC_BYTE);
	crc = crc8_getForByte(PROTO_SYNC_BYTE, PROTO_CRC8_POLY, crc);

	uart_send(code);
	crc = crc8_getForByte(code, PROTO_CRC8_POLY, crc);

	uart_send(bufferSize >> 8);
	crc = crc8_getForByte(bufferSize >> 8, PROTO_CRC8_POLY, crc);

	uart_send(bufferSize & 0xff);
	crc = crc8_getForByte(bufferSize & 0xff, PROTO_CRC8_POLY, crc);

	for (uint16_t i = 0; i < bufferSize; i++) {
		uart_send(buffer[i]);
		crc = crc8_getForByte(buffer[i], PROTO_CRC8_POLY, crc);
	}

	uart_send(crc);
}


int main(int argc, char *argv[]) {
	RxState rxState = RX_STATE_WAIT_SYNC;
	uint16_t pollTimeout = 0;

	uint8_t  pendingCmd      = 0;
	uint8_t  pendingCmdCrc   = 0;
	uint16_t pendingDataSize = 0;
	uint16_t pendingDataRead = 0;

	uint16_t toSend = 0;
	uint16_t toRecv = 0;

	uart_initialize();
	spi_initialize();

	while (1) {
		if (! uart_poll()) {
			pollTimeout++;

			if (pollTimeout >= 60000) {
				if (rxState != RX_STATE_WAIT_SYNC) {
					_response(PROTO_ERROR_TIMEOUT, NULL, 0);

					rxState = RX_STATE_WAIT_SYNC;
				}

				pollTimeout = 0;
			}

		} else {
			uint8_t byte = uart_get();

			pollTimeout = 0;

			switch (rxState) {
				case RX_STATE_WAIT_SYNC:
					{
						if (byte != PROTO_SYNC_BYTE) {
							_response(PROTO_ERROR_INVALID_SYNC_BYTE, NULL, 0);

						} else {
							rxState = RX_STATE_WAIT_CMD;

							pendingCmdCrc = crc8_getForByte(byte, PROTO_CRC8_POLY, 0);
						}
					}
					break;

				case RX_STATE_WAIT_CMD:
					{
						pendingCmd    = byte;
						pendingCmdCrc = crc8_getForByte(byte, PROTO_CRC8_POLY, pendingCmdCrc);
						rxState       = RX_STATE_WAIT_DATA_LEN_HI;
					}
					break;

				case RX_STATE_WAIT_DATA_LEN_HI:
					{
						pendingCmdCrc = crc8_getForByte(byte, PROTO_CRC8_POLY, pendingCmdCrc);
						rxState       = RX_STATE_WAIT_DATA_LEN_LO;

						pendingDataSize = ((uint16_t)(byte)) << 8;
					}
					break;

				case RX_STATE_WAIT_DATA_LEN_LO:
					{
						pendingDataSize |= byte;

						if (pendingDataSize > SPI_BUFFER_SIZE) {
							_response(PROTO_ERROR_INVALID_LENGTH, NULL, 0);

							rxState = RX_STATE_WAIT_SYNC;

						} else {
							pendingCmdCrc = crc8_getForByte(byte, PROTO_CRC8_POLY, pendingCmdCrc);

							if (byte == 0) {
								rxState = RX_STATE_WAIT_CRC;

							} else {
								rxState = RX_STATE_WAIT_DATA;

								pendingDataRead   = 0;
								_spiBufferWritten = 0;
							}
						}
					}
					break;

				case RX_STATE_WAIT_DATA:
					{
						if (pendingCmd == PROTO_CMD_SPI_TRANSFER) {
							switch (pendingDataRead) {
								case 0:
									{
										toSend  = ((uint16_t) byte) << 8;
									}
									break;

								case 1:
									{
										toSend |= byte;
									}
									break;

								case 2:
									{
										toRecv  = ((uint16_t) byte) << 8;
									}
									break;

								case 3:
									{
										toRecv |= byte;
									}
									break;

								default:
									_spiBuffer[_spiBufferWritten++] = byte;
							}
						}

						if (++pendingDataRead == pendingDataSize) {
							rxState = RX_STATE_WAIT_CRC;
						}

						pendingCmdCrc = crc8_getForByte(byte, PROTO_CRC8_POLY, pendingCmdCrc);
					}
					break;

				case RX_STATE_WAIT_CRC:
					{
						if (pendingCmdCrc != byte) {
							_response(PROTO_ERROR_INVALID_CRC, NULL, 0);

						} else {
							switch (pendingCmd) {
								case PROTO_CMD_SPI_CS_HI:
									{
										CS_HIGH();

										_response(PROTO_NO_ERROR, NULL, 0);
									}
									break;

								case PROTO_CMD_SPI_CS_LO:
									{
										CS_LOW();

										_response(PROTO_NO_ERROR, NULL, 0);
									}
									break;

								case PROTO_CMD_SPI_TRANSFER:
									{
										spi_transfer(_spiBuffer, toSend, toRecv);

										_response(PROTO_NO_ERROR, _spiBuffer, toRecv);
									}
									break;

								default:
									{
										_response(PROTO_ERROR_INVALID_CMD, NULL, 0);
									}
									break;
							}
						}

						rxState = RX_STATE_WAIT_SYNC;
					}
					break;
			}
		}
	}
}
