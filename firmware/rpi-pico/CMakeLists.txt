cmake_minimum_required(VERSION 3.12)

set(BASE_DIR ${CMAKE_CURRENT_LIST_DIR})

set(PICO_SDK_PATH ${BASE_DIR}/pico-sdk)

# Pull in SDK (must be before project)
include(${BASE_DIR}/cmake/pico_sdk_import.cmake)

# include(pico_extras_import_optional.cmake)

project(firmware_pico C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.3.0")
	message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.3.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

# Initialize the SDK
pico_sdk_init()

add_compile_options(
	-Wall
	-Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
	-Wno-unused-function # we have some for the docs that aren't called
	-Wno-maybe-uninitialized
)

add_executable(firmware
	${BASE_DIR}/src/main.c
	${BASE_DIR}/src/usb_descriptors.c
)

target_include_directories(firmware PRIVATE
	${BASE_DIR}/include
)

target_link_libraries(firmware
	PRIVATE 
		pico_stdlib
		hardware_uart
		hardware_pio
		hardware_spi
		tinyusb_device
		tinyusb_board
		
		firmware-common
		protocol
)

pico_add_extra_outputs(firmware)