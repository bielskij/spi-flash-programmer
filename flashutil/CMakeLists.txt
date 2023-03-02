cmake_minimum_required(VERSION 3.12)

project(flashutil)

set(CMAKE_CXX_STANDARD 11)

set(SRC_FILES 
	../common/src/crc8.c 

	src/serial.c
	src/debug.c
	src/main.c
)

add_executable(flash-util
	${SRC_FILES}
)

target_include_directories(flash-util PRIVATE
	include
	../common/include
)