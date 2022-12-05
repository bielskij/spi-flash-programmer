cmake_minimum_required(VERSION 3.12)

include(cmake/submodules.txt)
include(cmake/vcpkg.txt)

project(flashutil)

set(CMAKE_CXX_STANDARD 14)

find_package(Boost COMPONENTS thread program_options chrono REQUIRED)


file(GLOB_RECURSE SRC_FILES 
	../common/src/crc8.c
	src/common/debug.c
	src/main.cpp
	src/serial-boost.cpp
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