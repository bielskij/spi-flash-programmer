#!/usr/bin/env bash

AVR_UPLOAD_TOOL=avrdude
AVR_UPLOAD_PORT=/dev/ttyUSB0
AVR_UPLOAD_BAUDRATE=
AVR_UPLOAD_PROGRAMMER=arduino
AVR_UPLOAD_MCU=

function usage() {
	echo "Usage:"
	echo "   $0 <-m mcu> [-P programmer] [-p port] [-b baudrate]"
	
	exit 1
}

function fail() {
	echo $@
	exit 1
}


while getopts "hp:b:m:P:" arg; do
	case $arg in
		p)
			AVR_UPLOAD_PORT=$OPTARG
		;;
		
		P)
			AVR_UPLOAD_PROGRAMMER=$OPTARG
		;;
		
		b)
			AVR_UPLOAD_BAUDRATE=$OPTARG
		;;
		
		m)
			AVR_UPLOAD_MCU=$OPTARG
		;;
		
		*)
			usage
		;;
	esac
done

AVR_UPLOADTOOL_OPTS=""

if [ "x$AVR_UPLOAD_PORT" = "x" ]; then
	fail "AVR_UPLOAD_PORT is empty!"
else
	AVR_UPLOADTOOL_OPTS+=" -P $AVR_UPLOAD_PORT"
fi

if [ "x$AVR_UPLOAD_BAUDRATE" != "x" ]; then
	AVR_UPLOADTOOL_OPTS+=" -b $AVR_UPLOAD_BAUDRATE"
fi

if [ "x$AVR_UPLOAD_PROGRAMMER" = "x" ]; then
	fail "AVR_UPLOAD_PROGRAMMER is empty!"
else
	AVR_UPLOADTOOL_OPTS+=" -c $AVR_UPLOAD_PROGRAMMER"
fi

if [ "x$AVR_UPLOAD_MCU" = "x" ]; then
	fail "AVR_UPLOAD_MCU is empty!"
else
	AVR_UPLOADTOOL_OPTS+=" -p $AVR_UPLOAD_MCU"
fi

$AVR_UPLOAD_TOOL $AVR_UPLOADTOOL_OPTS