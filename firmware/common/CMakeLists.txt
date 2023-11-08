cmake_minimum_required(VERSION 3.16)

project(firmware-common)

set(src_path "${CMAKE_CURRENT_LIST_DIR}/src/")
set(srcs
	${src_path}/firmware/programmer.c
)

set(headers_path "${CMAKE_CURRENT_LIST_DIR}/include/")
set(headers
	${headers_path}/firmware/programmer.h
)

add_library(firmware-common ${headers} ${srcs})

target_include_directories(firmware-common
	PUBLIC
		${headers_path}
	PRIVATE
		$<TARGET_PROPERTY:protocol,INCLUDE_DIRECTORIES>
)