set(DEFAULT_AVR_UPLOADTOOL_PORT     /dev/ttyUSB0)
set(DEFAULT_AVR_UPLOADTOOL_BAUDRATE 57600)
set(DEFAULT_AVR_PROGRAMMER          arduino)

set(DEFAULT_FW_F_CPU  16000000)
set(DEFAULT_FW_BAUD   500000)

find_program(AVR_CC        avr-gcc     REQUIRED)
find_program(AVR_CXX       avr-g++     REQUIRED)
find_program(AVR_OBJCOPY   avr-objcopy REQUIRED)
find_program(AVR_SIZE_TOOL avr-size    REQUIRED)
find_program(AVR_OBJDUMP   avr-objdump REQUIRED)

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR avr)
set(CMAKE_C_COMPILER       ${AVR_CC})
set(CMAKE_CXX_COMPILER     ${AVR_CXX})

project(arduino-firmare)

find_program(AVR_UPLOADTOOL avrdude)

set(AVR_MCU atmega328p)
set(AVR_L_FUSE 0xF7)
set(AVR_H_FUSE 0xD9)
set(AVR_E_FUSE 0xFF) 

set(BURN_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/burn.sh)

if(NOT AVR_UPLOADTOOL_PORT)
	set(AVR_UPLOADTOOL_PORT ${DEFAULT_AVR_UPLOADTOOL_PORT} CACHE STRING "Set default upload tool port: ${DEFAULT_AVR_UPLOADTOOL_PORT}")
endif()

if(NOT AVR_UPLOADTOOL_BAUDRATE)
	set(AVR_UPLOADTOOL_BAUDRATE ${DEFAULT_AVR_UPLOADTOOL_BAUDRATE} CACHE STRING "Set default programmer baudrate: ${DEFAULT_AVR_UPLOADTOOL_BAUDRATE}")
endif()

if(NOT AVR_PROGRAMMER)
	set(AVR_PROGRAMMER ${DEFAULT_AVR_PROGRAMMER} CACHE STRING "Set default programmer hardware model: ${DEFAULT_AVR_PROGRAMMER}")
endif()

if(NOT FW_F_CPU)
	set(FW_F_CPU ${DEFAULT_FW_F_CPU} CACHE STRING "Set default cpu frequency: ${DEFAULT_FW_F_CPU}")
endif()

if(NOT FW_BAUD)
	set(FW_BAUD ${DEFAULT_FW_BAUD} CACHE STRING "Set default serial baudrate: ${DEFAULT_FW_BAUD}")
endif()

set(BURN_SCRIPT_BASE_OPTIONS -m ${AVR_MCU} -P ${AVR_PROGRAMMER} -p ${AVR_UPLOADTOOL_PORT})

if(AVR_UPLOADTOOL_BAUDRATE)
	set(BURN_SCRIPT_BASE_OPTIONS ${BURN_SCRIPT_BASE_OPTIONS} -b ${AVR_UPLOADTOOL_BAUDRATE})
endif()

set(AVR_SIZE_ARGS -C;--mcu=${AVR_MCU})

set(EXECUTABLE_NAME firmware)

set(elf_file ${EXECUTABLE_NAME}.elf)
set(hex_file ${EXECUTABLE_NAME}.hex)
set(lst_file ${EXECUTABLE_NAME}.lst)
set(map_file ${EXECUTABLE_NAME}.map)
set(eeprom_image ${EXECUTABLE_NAME}-eeprom.hex)

add_definitions("-DF_CPU=${FW_F_CPU}")
add_definitions("-mmcu=${AVR_MCU}")

add_definitions("-std=c99")

add_definitions("-O2")
add_definitions("-mrelax")

add_definitions("-Wall")
add_definitions("-Wundef")
add_definitions("-Warray-bounds")
add_definitions("-Wformat")
add_definitions("-Wmissing-braces")
add_definitions("-Wreturn-type")

add_definitions("-fshort-enums")
add_definitions("-fpack-struct")
add_definitions("-fno-split-wide-types")
add_definitions("-ffunction-sections")
add_definitions("-fdata-sections")

add_definitions("-DBAUD=${FW_BAUD}")

link_libraries("-mmcu=${AVR_MCU} -Wl,--gc-sections -mrelax -Wl,-Map,${map_file}")

set(src_path "${CMAKE_CURRENT_LIST_DIR}/src/")

add_executable(${elf_file}
	${src_path}/main.c
)

target_link_libraries(${elf_file}
	PRIVATE
		firmware-common
		protocol
)

add_custom_command(
	OUTPUT 
		${hex_file}
	COMMAND
		${AVR_OBJCOPY} -j .text -j .data -O ihex ${elf_file} ${hex_file}
	COMMAND
		${AVR_SIZE_TOOL} ${AVR_SIZE_ARGS} ${elf_file}
	DEPENDS 
		${elf_file}
)

add_custom_command(
	OUTPUT
		${lst_file}
	COMMAND
		${AVR_OBJDUMP} -d ${elf_file} > ${lst_file}
	DEPENDS 
		${elf_file}
)

add_custom_command(
	OUTPUT 
		${eeprom_image}
	COMMAND
		${AVR_OBJCOPY} -j .eeprom --set-section-flags=.eeprom=alloc,load --change-section-lma .eeprom=0 --no-change-warnings -O ihex ${elf_file} ${eeprom_image}
	DEPENDS 
		${elf_file}
)

add_custom_target(
	${EXECUTABLE_NAME}
	ALL
	DEPENDS 
		${hex_file} ${lst_file} ${eeprom_image}
)

add_custom_target(
	upload_${EXECUTABLE_NAME}
		${BURN_SCRIPT} ${BURN_SCRIPT_BASE_OPTIONS} -f ${hex_file}

	DEPENDS 
		${hex_file}

	COMMENT 
		"Uploading ${hex_file} to ${AVR_MCU} using ${AVR_PROGRAMMER}"
)

add_custom_target(
	upload_${EXECUTABLE_NAME}_eeprom
		${BURN_SCRIPT} ${BURN_SCRIPT_BASE_OPTIONS} -e ${eeprom_image} 
		
	DEPENDS 
		${eeprom_image}

	COMMENT 
		"Uploading ${eeprom_image} to ${AVR_MCU} using ${AVR_PROGRAMMER}"
)


add_custom_target(
	set_fuses
		${BURN_SCRIPT} ${BURN_SCRIPT_BASE_OPTIONS} -L ${AVR_L_FUSE} -H ${AVR_H_FUSE} -E ${AVR_E_FUSE}
		
	COMMENT
		"Setup: High Fuse: ${AVR_H_FUSE} Low Fuse: ${AVR_L_FUSE}, Extended Fuse: ${AVR_E_FUSE}"
)