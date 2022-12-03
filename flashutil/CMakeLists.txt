cmake_minimum_required(VERSION 3.14)

include(cmake/submodules.txt)
include(cmake/vcpkg.txt)

project(flashutil)

find_package(Boost REQUIRED)


file(GLOB_RECURSE SRC_FILES 
	../common/src/crc8.c
	src/debug.c
	src/main.c
	src/serial-boost.cpp
)

add_executable(flash-util
	${SRC_FILES}
)

target_include_directories(flash-util PRIVATE
	include
	../common/include
)

target_link_libraries(flash-util PRIVATE
	Boost::boost
)