cmake_minimum_required(VERSION 3.12)

set(PICO_SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/pico-sdk)

# Pull in SDK (must be before project)
include(cmake/pico_sdk_import.cmake)

# include(pico_extras_import_optional.cmake)

project(firmware_pico C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.3.0")
	message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.3.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

# Initialize the SDK
pico_sdk_init()

#include(example_auto_set_url.cmake)
# Add blink example
#add_subdirectory(blink)

# Add hello world example
#add_subdirectory(hello_world)

add_compile_options(
	-Wall
	-Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
	-Wno-unused-function # we have some for the docs that aren't called
	-Wno-maybe-uninitialized
)

add_executable(firmware
	src/main.c
	src/usb_descriptors.c

	../common/src/firmware/programmer.c
	../../common/src/crc8.c
)

target_include_directories(firmware PRIVATE
	include

	../common/include
	../../common/include
)

target_link_libraries(firmware PRIVATE 
	pico_stdlib
	hardware_uart
	hardware_pio
	hardware_spi
	tinyusb_device
	tinyusb_board
)

pico_add_extra_outputs(firmware)