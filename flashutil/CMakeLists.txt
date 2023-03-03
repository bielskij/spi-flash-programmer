cmake_minimum_required(VERSION 3.12)

project(flashutil)

set(CMAKE_CXX_STANDARD 11)

find_package(Boost COMPONENTS program_options REQUIRED)

set(SRC_FILES 
	../common/src/crc8.c 

	src/serial.c
	src/debug.c
	src/main.cpp
)

add_executable(flash-util
	${SRC_FILES}
)

target_include_directories(flash-util PRIVATE
	include
	../common/include
)

target_link_libraries(flash-util PUBLIC
	Boost::program_options
)