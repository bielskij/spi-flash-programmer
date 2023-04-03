#!/usr/bin/env bash

set -e

SERIAL_PORT=${SERIAL_PORT:-/dev/ttyUSB0}
SERIAL_BAUD=${SERIAL_BAUD:-500000}

FLASH_UTIL="${FLASH_UTIL:-../build/flash-util}"

PAGE_SIZE=256

TEST_BLOCK_FIRST=0
TEST_BLOCK_LAST=
TEST_SECTOR_FIRST=0
TEST_SECTOR_LAST=
TEST_PAGE_FIRST=0
TEST_PAGE_LAST=

QUIET="> /dev/null 2>&1"

if [ "${V}" = "1" ]; then
	QUIET=
fi

function log() {
	echo -e "##### log: $@"
}

function cleanup() {
	echo "Cleaning up"

return 0	
	if [ "x${FLASH_TEST_PATH}" != "x" ]; then
		rm -f ${FLASH_TEST_PATH}
	fi
	
	if [ "x${BLOCK_TEST_PATH}" != "x" ]; then
		rm -f ${BLOCK_TEST_PATH}
	fi
	
	if [ "x${SECTOR_TEST_PATH}" != "x" ]; then
		rm -f ${SECTOR_TEST_PATH}
	fi
	
	if [ "x${PAGE_TEST_PATH}" != "x" ]; then
		rm -f ${PAGE_TEST_PATH}
	fi
	
	if [ "x${TMP_PATH}" != "x" ]; then
		rm -f ${TMP_PATH}
	fi
}

FLASH_UTIL_READER_PARAM="-s ${SERIAL_PORT} --serial-baud ${SERIAL_BAUD}"

function callCmd() {
	local cmd="$@"
	
	log "calling \"$cmd\""
	
	eval "$cmd ${QUIET} 2>&1"
}

function callFlashUtil() {
	local cmd="${FLASH_UTIL} ${FLASH_UTIL_READER_PARAM} $@"

	callCmd "$cmd"
}

UTIL_OUT=$(QUIET= callFlashUtil)

FLASH_PAGE_SIZE=256
FLASH_SIZE=$(echo "${UTIL_OUT}" | sed -n "s/^.*size: \([0-9]\+\)B.*$/\1/p")
FLASH_BLOCKS=$(echo "${UTIL_OUT}" | sed -n "s/^.*blocks: \([0-9]\+\) .*$/\1/p")
FLASH_SECTORS=$(echo "${UTIL_OUT}" | sed -n "s/^.*sectors: \([0-9]\+\) .*$/\1/p")
FLASH_PAGES=$(( ${FLASH_SIZE} / ${FLASH_PAGE_SIZE} ))
FLASH_BLOCK_SIZE=$(( ${FLASH_SIZE} / ${FLASH_BLOCKS} ))
FLASH_SECTOR_SIZE=$(( ${FLASH_SIZE} / ${FLASH_SECTORS} ))

FLASH_SECTORS_PER_BLOCK=$(( ${FLASH_SECTORS} / ${FLASH_BLOCKS} ))
FLASH_PAGES_PER_SECTOR=$(( ${FLASH_SECTOR_SIZE} / ${FLASH_PAGE_SIZE} ))

TEST_BLOCK_LAST=$(( ${FLASH_BLOCKS} - 1 ))
TEST_SECTOR_LAST=$(( ${FLASH_SECTORS_PER_BLOCK} - 1 ))
TEST_PAGE_LAST=$(( ${FLASH_PAGES_PER_SECTOR} - 1 ))

echo -e "\n##"
echo -e "## FLASH_SIZE:       ${FLASH_SIZE} Bytes"
echo -e "## FLASH_BLOCKS:     ${FLASH_BLOCKS}"
echo -e "## FLASH_SECTORS:    ${FLASH_SECTORS}"
echo -e "## FLASH_PAGES:      ${FLASH_PAGES}"
echo -e "## BLOCK_SIZE:       ${FLASH_BLOCK_SIZE}"
echo -e "## SECTOR_SIZE:      ${FLASH_SECTOR_SIZE}"
echo -e "## PAGE_SIZE:        ${FLASH_PAGE_SIZE}"
echo -e "##"
echo -e "## SECTORS_PER_BLOCK ${FLASH_SECTORS_PER_BLOCK}"
echo -e "## PAGES_PER_SECTOR  ${FLASH_PAGES_PER_SECTOR}"
echo -e "##"
echo -e "## TEST_BLOCKS:  ${TEST_BLOCK_FIRST}, ${TEST_BLOCK_LAST}"
echo -e "## TEST_SECTORS: ${TEST_SECTOR_FIRST}, ${TEST_SECTOR_LAST}"
echo -e "## TEST_PAGES:   ${TEST_PAGE_FIRST}, ${TEST_PAGE_LAST}"
echo -e "##\n"

trap cleanup EXIT

FLASH_TEST_PATH=$(mktemp)
BLOCK_TEST_PATH=$(mktemp)
SECTOR_TEST_PATH=$(mktemp)
PAGE_TEST_PATH=$(mktemp)

TMP_PATH=$(mktemp)

head -c ${FLASH_PAGE_SIZE}   /dev/urandom > ${PAGE_TEST_PATH}
head -c ${FLASH_SECTOR_SIZE} /dev/urandom > ${SECTOR_TEST_PATH}
head -c ${FLASH_BLOCK_SIZE}  /dev/urandom > ${BLOCK_TEST_PATH}
head -c ${FLASH_SIZE}        /dev/urandom > ${FLASH_TEST_PATH}

function test_erase_write_block() {
	log "\n------------------------- TEST, BLOCK: erase, write, read"
	
	local blocks=( ${TEST_BLOCK_FIRST} ${TEST_BLOCK_LAST} )
	
	for block in ${blocks[@]}; do
		log "Erasing, writing and verifying test block $block"
		
		rm -f ${TMP_PATH}
	
		callFlashUtil --erase-block ${block} --no-redudant-cycles
		callFlashUtil --write-block ${block} -i ${BLOCK_TEST_PATH} -V
		callFlashUtil --read-block  ${block} -o ${TMP_PATH}
		
		if ! diff ${BLOCK_TEST_PATH} ${TMP_PATH}; then
			log "\nReference sector\n"
			hexdump -C ${SECTOR_TEST_PATH}
			log "\nRead sector\n" 
			hexdump -C ${TMP_PATH}
			return 1
		fi
	done
}

function test_erase_write_sector() {
	log "\n------------------------- TEST, SECTOR: erase, write, read"
	
	local block=${TEST_BLOCK_FIRST}
	local sectors=(${TEST_SECTOR_FIRST} ${TEST_SECTOR_LAST})

	callFlashUtil --erase-block ${block} --no-redudant-cycles

	for sector in ${sectors[@]}; do
		local dstSector=$(( ${block} * ${FLASH_SECTORS_PER_BLOCK} + ${sector} ))
		
		log "Erasing, writing and verifying test sector $dstSector"
		
		rm -f ${TMP_PATH}
	
		callFlashUtil --erase-sector ${dstSector} --no-redudant-cycles
		callFlashUtil --write-sector ${dstSector} -i ${SECTOR_TEST_PATH} -V
		callFlashUtil --read-sector  ${dstSector} -o ${TMP_PATH}

		if ! diff ${SECTOR_TEST_PATH} ${TMP_PATH}; then
			log "\nReference sector\n"
			hexdump -C ${SECTOR_TEST_PATH}
			log "\nRead sector\n" 
			hexdump -C ${TMP_PATH}
			return 1
		fi
	done

	log "Verifying whole block"
	{
		head -c ${FLASH_BLOCK_SIZE}  /dev/zero | tr "\000" "\377" > ${BLOCK_TEST_PATH}

		cat ${SECTOR_TEST_PATH} | dd of=${BLOCK_TEST_PATH} bs=1 conv=notrunc seek=$(( ${block} * ${FLASH_BLOCK_SIZE} + ${TEST_SECTOR_FIRST} * ${FLASH_SECTOR_SIZE} ))
		cat ${SECTOR_TEST_PATH} | dd of=${BLOCK_TEST_PATH} bs=1 conv=notrunc seek=$(( ${block} * ${FLASH_BLOCK_SIZE} + ${TEST_SECTOR_LAST}  * ${FLASH_SECTOR_SIZE} ))
	}
	
	callFlashUtil --read-block ${block} -o ${TMP_PATH}
	
	if ! diff ${BLOCK_TEST_PATH} ${TMP_PATH}; then
		log "\nReference block\n"
		hexdump -C ${SECTOR_TEST_PATH}
		log "\nRead block\n" 
		hexdump -C ${TMP_PATH}
		return 1
	fi
}

function test_erase_write_flash() {
	log "\n------------------------- TEST, FLASH: erase, write, read"

	rm -f ${TMP_PATH}

	log "Erasing whole chip"
	callFlashUtil --erase --no-redudant-cycles
	
	log "Writing test image"
	callFlashUtil --write -i ${FLASH_TEST_PATH} -V
	
	log "Reading whole chip"
	callFlashUtil --read -o ${TMP_PATH}

	diff ${FLASH_TEST_PATH} ${TMP_PATH}
}

test_erase_write_block
test_erase_write_sector
test_erase_write_flash

log "SUCCESS"
