# Arduino SPI flash programmer.

This project is an another SPI flash programmer. I need a SPI flash programmer to burn a flash chip to repair LCD monitor. As I hadn't any programmer I decided to make one by my own using Arduino nano. 

As a result a complete solution was implemented.

## Features.
  * universal SPI flash programmer
    * Predefined chips:
        * MX25L2026E
        * MX25l2005A
        * MX25V16066
        * W25Q32
        * W25Q80
    * Generic chips
        * easy configurable with ``--flash-geometry`` parameter
  * unprotect operation
  * erase/write verification
  * communication protocol secured by CRC checksum
  * high-speed SPI (8Mhz)
  * high-speed USART (1Mbaud)

## Arduino connections.
  * D10 (PB2) ------ CS   (chip select)
  * D11 (PB3) ------ MOSI (SPI data output)
  * D12 (PB4) ------ MISO (SPI data input)
  * D13 (PB5) ------ SCK  (SPI clock)

**NOTE** For communication with 3.3V chips simple resistor voltage divider can be used. In my case voltage divider using 1K and 2K resistors was enough.

## Flash connection.
  * WE (write enable) pin should be connected to ground

## Building and writing Arduino firmware.
```
cd firmware
make clean all burn
```

## Writing precompiled images.

**NOTE** Prebuilt firmware is available at ``firmware/dist/firmware_<clk>_<baud>.hex``

```
cd firmware
make HEX_FILE=dist/firmware_16mhz_19200bps.hex burn
```

By default, **burn** target writes the firmware using avrdude via arduino programmer (arduino resistant bootloader). There is an other target **burn-usbasp** for writing firmware to raw atmega328p (USBasp programmer, ISP connection).

## Building flashutil.
```
cd flashutil
make clean all
```

## Use cases
  * Print help and exit
```
flash-util -h
```
  * Erase whole chip (without verification)
```
flash-util -p /dev/ttyUSB0 -E
```
  * Erase whole chip (with verification)
```
flash-util -p /dev/ttyUSB0 -E -V
```
  * Write image to chip (with verification)
```
flash-util -p /dev/ttyUSB0 -W -i flash_image.bin -V
```
  * Read whole chip
```
flash-util -p /dev/ttyUSB0 -R -o /tmp/flash.bin 
```
  * Writing image to unknown chip
```
flash-util -p /dev/ttyUSB0 -E -V --flash-geometry  65536:64:4096:1024:fc
```
