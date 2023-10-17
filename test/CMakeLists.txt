cmake_minimum_required(VERSION 3.14)
project(unit_tests_project)

# GoogleTest requires at least C++14
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

file(GLOB_RECURSE TEST_FILES src/*.cpp) 

add_executable(unit_tests
	../common/src/common/crc8.c
	
	../common/src/common/protocol/common.c
	../common/src/common/protocol/packet.c
	../common/src/common/protocol/request.c
	../common/src/common/protocol/response.c
	
	../firmware/common/src/firmware/programmer.c
	
	${TEST_FILES}
)

message(${TEST_FILES})

# 

target_include_directories(unit_tests 
	PRIVATE
		include
		../common/include
		../firmware/common/include
		../flashutil/include
)

target_link_libraries(unit_tests
	GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(unit_tests)