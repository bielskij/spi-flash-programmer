#!/bin/env bash

VERBOSE=0

function build() {
	local fcpu=$1; shift
	local baud=$1; shift
	local outDir=$1; shift

	make V=${VERBOSE} F_CPU=${fcpu} BAUD=${baud} -f Makefile clean all
	
	cp -f out/firmware.hex ${outDir}/firmware_$(( fcpu / 1000000 ))mhz_${baud}bps.hex
}

build 16000000 1000000 dist
build 16000000  500000 dist
build 16000000   38400 dist
build 16000000   19200 dist

build  8000000 1000000 dist
build  8000000  500000 dist
build  8000000   38400 dist
build  8000000   19200 dist