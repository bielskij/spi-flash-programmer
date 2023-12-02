#!/usr/bin/env bash

set -e

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

VERBOSE=0

function build() {
	local fcpu=$1; shift
	local baud=$1; shift
	local outDir=$1; shift

	rm -rf build; mkdir build
	pushd build
		cmake ../ -DFW_F_CPU=${fcpu} -DFW_BAUD=${baud}
		VERBOSE=${VERBOSE} make -j8 clean all
		cp -f firmware.hex ${outDir}/firmware_$(( fcpu / 1000000 ))mhz_${baud}bps.hex
	popd
	rm -rf build;
}

build 16000000  500000 ${SCRIPT_DIR}/dist
build 16000000   38400 ${SCRIPT_DIR}/dist
build 16000000   19200 ${SCRIPT_DIR}/dist

build  8000000  500000 ${SCRIPT_DIR}/dist
build  8000000   38400 ${SCRIPT_DIR}/dist
build  8000000   19200 ${SCRIPT_DIR}/dist
