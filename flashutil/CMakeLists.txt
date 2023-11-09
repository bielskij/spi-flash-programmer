cmake_minimum_required(VERSION 3.12)

project(flashutil)

set(CMAKE_CXX_STANDARD 14)

find_package(Boost COMPONENTS program_options thread chrono REQUIRED)
find_package(nlohmann_json REQUIRED)

if(NOT TARGET spdlog)
	find_package(spdlog REQUIRED)
endif()

set(src_path     "${CMAKE_CURRENT_LIST_DIR}/src/flashutil")
set(headers_path "${CMAKE_CURRENT_LIST_DIR}/include/")

add_library(flashutil SHARED
	${src_path}/spi.cpp
	${src_path}/spi/serial.cpp

	${src_path}/serial/hw.cpp

	${src_path}/debug.c
	${src_path}/entryPoint.cpp
	${src_path}/flash.cpp
	${src_path}/programmer.cpp
	${src_path}/spi.cpp
	
	${src_path}/flash/builder.cpp
	${src_path}/flash/status.cpp
	${src_path}/flash/registry.cpp
	${src_path}/flash/registry/reader/json.cpp
)

target_include_directories(flashutil PUBLIC
	${headers_path}
)

target_link_libraries(flashutil PUBLIC
	protocol
)

add_executable(flash-util-app
	${src_path}/main.cpp
)

target_include_directories(flash-util-app PRIVATE
	include
)

target_link_libraries(flash-util-app 
	PUBLIC
		flashutil 
		Boost::thread 
		Boost::program_options 
		Boost::chrono 
		nlohmann_json::nlohmann_json
)
