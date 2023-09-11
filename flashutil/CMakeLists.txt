cmake_minimum_required(VERSION 3.12)

project(flashutil)

set(CMAKE_CXX_STANDARD 11)

find_package(Boost COMPONENTS program_options thread chrono REQUIRED)

set(SRC_FILES 
	../common/src/common/crc8.c 

	src/spi.cpp
	src/spi/creator.cpp
	src/spi/impl/serial.cpp
	src/spi/impl/serial/serial.cpp
	
	src/flash.cpp
	src/flash/builder.cpp
	src/flash/registry.cpp
	
	src/programmer.cpp
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
	Boost::thread Boost::program_options Boost::chrono
)