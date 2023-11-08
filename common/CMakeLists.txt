cmake_minimum_required(VERSION 3.0)
project(protocol)

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/version.txt)

# https://www.foonathan.net/2016/03/cmake-install/

set(protocol_VERSION_MAJOR ${VERSION_MAJOR} CACHE STRING "major version" FORCE)
set(protocol_VERSION_MINOR ${VERSION_MINOR} CACHE STRING "minor version" FORCE)
set(protocol_VERSION_PATCH ${VERSION_PATCH} CACHE STRING "minor version" FORCE)
set(protocol_VERSION ${protocol_VERSION_MAJOR}.${protocol_VERSION_MINOR} CACHE STRING "version" FORCE)

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

add_library(protocol ${headers} ${srcs})

target_include_directories(protocol
	PUBLIC
		${headers_path}
)

install(TARGETS protocol
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin
)


#set(include_dest "include/my_library-${MY_LIBRARY_VERSION}")
#set(main_lib_dest "lib/my_library-${MY_LIBRARY_VERSION}")

#install(TARGETS my_library DESTINATION "${main_lib_dest}")
#install(FILES ${headers} DESTINATION "${include_dest}")
#install(FILES ${CMAKE_CURRENT_BINARY_DIR}/config_impl.hpp DESTINATION "${include_dest}")