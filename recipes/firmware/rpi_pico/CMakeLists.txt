cmake_minimum_required(VERSION 3.16)

find_package(Git QUIET)

if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/../../../.git")
# Update submodules as needed
	option(GIT_SUBMODULE "Check submodules during build" ON)
	if(GIT_SUBMODULE)
		message(STATUS "Submodule update")
		execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
			RESULT_VARIABLE GIT_SUBMOD_RESULT)
		if(NOT GIT_SUBMOD_RESULT EQUAL "0")
			message(FATAL_ERROR "git submodule update --init --recursive failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
		endif()
	endif()
endif()

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/../../../firmware/rpi-pico/pico-sdk/README.md")
	message(FATAL_ERROR "The submodules were not downloaded! GIT_SUBMODULE was turned off or failed. Please update submodules and try again.")
endif()

include(${CMAKE_SOURCE_DIR}/../../../firmware/rpi-pico/CMakeLists.txt)

project(recipe-firmware-rpi-pico)

include(../../../common/CMakeLists.txt)
include(../../../firmware/common/CMakeLists.txt)