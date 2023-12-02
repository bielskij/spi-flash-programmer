# SPI flash programmer.

This project is an another SPI flash programmer. I need a SPI flash programmer to burn a flash chip to repair LCD monitor. As I hadn't any programmer I decided to make one by my own using Arduino nano. 

As a result a complete solution was implemented.

## Features.
  * supported modules:
    * Arduino nano compatible
    * RPi Pico (RP2040)
  * universal SPI flash programmer
    * Predefined chips:
        * MX25L2026E
        * MX25l2005A
        * MX25V16066
        * W25Q32
        * W25Q80
    * Generic chips
        * easy configurable with ``--flash-geometry`` parameter
        * easy configurable via ``chips.json``
  * unprotect operation
  * erase/write verification
  * communication protocol secured by CRC checksum
  * high-speed SPI 
    * 8MHz - Arduino
    * 60MHz - RPi Pico
  * high-speed USART/CDC
    * 1Mbaud - Arduino
    * 6Mbit - RPi Pico

## Arduino

### Arduino connections.
  * D10 (PB2) ------ CS   (chip select)
  * D11 (PB3) ------ MOSI (SPI data output)
  * D12 (PB4) ------ MISO (SPI data input)
  * D13 (PB5) ------ SCK  (SPI clock)

**NOTE** For communication with 3.3V chips simple resistor voltage divider can be used. In my case voltage divider using 1K and 2K resistors was enough.

## Flash connection.
  * WE (write enable) pin should be connected to ground

## Building and writing Arduino firmware.
```
mkdir build && cd build
cmake -S ../recipes/firmware/arduino/ -B .
make all upload_firmware
```

## Writing precompiled images.

**NOTE** Prebuilt firmware is available at ``firmware/arduino/dist/firmware_<clk>_<baud>.hex``

```
cd firmware/arduino
./burn.sh -m atmega328p -f dist/firmware_16mhz_19200bps.hex
```

By default, **burn.sh** scripts writes the firmware using **avrdude** via **arduino** programmer (arduino resistant bootloader). Programmer type, MCU and optional communication baudrate can be adjusted for particular hardware configuration. Please find the following syntax of burn.sh script:

```
Usage:
   ./burn.sh <-m mcu> [-P programmer] [-p port] [-b baudrate] [-m mcu] [-L lfuse] [-H hfuse] [-E efuse] [-f flash.hex] [-e e2prom.hex]
```

## RPi-Pico

### Linux Environment preparation.

```
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib
```

###  Building and writing firmware.

```
cd firmware/rpi-pico
git submodule init
git submodule update

cd pico-sdk
git submodule init
git submodule update

cd --
mkdir build && cd build
cmake ..
make all
```

### RPi-Pico connections.
  * 21 (GP16) ------ MISO (SPI data input)
  * 22 (GP17) ------ CS   (chip select)
  * 24 (GP18) ------ SCK  (SPI clock)
  * 25 (GP19) ------ MOSI (SPI data output)


## Building flashutil.
```
mkdir build && cd build
cmake ../recipes/flashutil/
make all
```

### Flash chips repository.
There is a predefined list of flash chips added to this project at ``flashutil/etc/chips.json``. It contains declaractions (geomtry) of all chips mentioned in the 'Features' section. 
Other custom chips can be added by analogy without adding ``-g`` option to ``flash-util`` call.

All size-related values are string values and optionally support binary metric modifiers such  as ``Kib``, ``Mib``, ``Gib`` (representing kibibit, mebibit, gigibit), as well as ``KiB``, ``MiB``, ``GiB`` (representing kibibyte, mebibyte, gigibyte).

## Use cases
  * Print help and exit
```
flash-util -h
```
  * Erase whole chip (without verification)
```
flash-util -s /dev/ttyUSB0 -R ../flashutil/etc/chips.json -e
```
  * Erase whole chip (with verification)
```
flash-util -s /dev/ttyUSB0 -R ../flashutil/etc/chips.json -e -V
```
  * Write image to chip (with verification)
```
flash-util -s /dev/ttyUSB0 -R ../flashutil/etc/chips.json -w -i /tmp/flash.src.bin -V
```
  * Read whole chip
```
flash-util -s /dev/ttyUSB0 -R ../flashutil/etc/chips.json -r -o /tmp/flash.dst.bin
```
  * Writing image to unknown chip
```
flash-util -s /dev/ttyUSB0 -e -w -i /tmp/flash.src.bin -V --flash-geometry  65536:64:4096:1024:fc
```