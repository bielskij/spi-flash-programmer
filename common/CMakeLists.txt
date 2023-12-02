cmake_minimum_required(VERSION 3.0)
project(protocol)

set(headers_path "${CMAKE_CURRENT_LIST_DIR}/include/")
set(headers
	${headers_path}/common/crc8.h
	${headers_path}/common/protocol.h
	${headers_path}/common/protocol/packet.h
	${headers_path}/common/protocol/request.h
	${headers_path}/common/protocol/response.h
)

set(src_path "${CMAKE_CURRENT_LIST_DIR}/src/common")
set(srcs
	${src_path}/protocol/common.c
	${src_path}/protocol/packet.c
	${src_path}/protocol/request.c
	${src_path}/protocol/response.c
	${src_path}/crc8.c
)

add_library(protocol 
	${headers} ${srcs}
)

target_include_directories(protocol
	PUBLIC
		${headers_path}
)