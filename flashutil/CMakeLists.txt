cmake_minimum_required(VERSION 3.12)

project(flashutil)

set(CMAKE_CXX_STANDARD 14)

find_package(Boost COMPONENTS program_options thread chrono REQUIRED)

set(SRC_FILES 
	../common/src/common/crc8.c 

	src/flashutil/spi.cpp
	src/flashutil/spi/creator.cpp
	src/flashutil/spi/impl/serial.cpp
	src/flashutil/spi/impl/serial/serial.cpp
	
	src/flashutil/flash.cpp
	src/flashutil/flash/builder.cpp
	src/flashutil/flash/registry.cpp
	
	src/flashutil/programmer.cpp
	src/flashutil/debug.c
	src/flashutil/main.cpp
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
