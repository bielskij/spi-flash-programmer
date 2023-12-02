#!/usr/bin/env bash

AVR_UPLOAD_TOOL=avrdude
AVR_UPLOAD_PORT=/dev/ttyUSB0
AVR_UPLOAD_BAUDRATE=
AVR_UPLOAD_PROGRAMMER=arduino
AVR_UPLOAD_MCU=

AVR_UPLOAD_HFUSE=
AVR_UPLOAD_LFUSE=
AVR_UPLOAD_EFUSE=
AVR_UPLOAD_FIRMWARE=

function usage() {
	echo "Usage:"
	echo "   $0 <-m mcu> [-P programmer] [-p port] [-b baudrate] [-m mcu] [-L lfuse] [-H hfuse] [-E efuse] [-f flash.hex] [-e e2prom.hex]"
	
	exit 1
}

function fail() {
	echo $@
	exit 1
}


while getopts "hp:b:m:P:L:H:E:f:e:" arg; do
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
		
		L)
			AVR_UPLOAD_LFUSE=$OPTARG
		;;
		
		H)
			AVR_UPLOAD_HFUSE=$OPTARG
		;;
		
		E)
			AVR_UPLOAD_EFUSE=$OPTARG
		;;
		
		f)
			AVR_UPLOAD_FIRMWARE=$OPTARG
		;;
		
		e)
			AVR_UPLOAD_E2PROM=$OPTARG
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

if [ "x$AVR_UPLOAD_LFUSE" != "x" ]; then
	AVR_UPLOADTOOL_OPTS+=" -U lfuse:w:${AVR_UPLOAD_LFUSE}:m"
fi

if [ "x$AVR_UPLOAD_HFUSE" != "x" ]; then
	AVR_UPLOADTOOL_OPTS+=" -U hfuse:w:${AVR_UPLOAD_HFUSE}:m"
fi

if [ "x$AVR_UPLOAD_EFUSE" != "x" ]; then
	AVR_UPLOADTOOL_OPTS+=" -U efuse:w:${AVR_UPLOAD_EFUSE}:m"
fi

if [ "x$AVR_UPLOAD_FIRMWARE" != "x" ]; then
	AVR_UPLOADTOOL_OPTS+=" -U flash:w:${AVR_UPLOAD_FIRMWARE}"
fi

if [ "x$AVR_UPLOAD_E2PROM" != "x" ]; then
	AVR_UPLOADTOOL_OPTS+=" -U eeprom:w:${AVR_UPLOAD_E2PROM}"
fi

cmd="$AVR_UPLOAD_TOOL $AVR_UPLOADTOOL_OPTS"

echo "Executing '$cmd'"

eval $cmd