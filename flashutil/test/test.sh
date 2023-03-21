#!/usr/bin/env bash

set -e

SERIAL_PORT=/dev/ttyUSB0
SERIAL_BAUD=1000000

FLASH_UTIL="${FLASH_UTIL:-../build/flash-util}"

PAGE_SIZE=256

TEST_BLOCK_IDX=2
TEST_PAGE_0_OFFSET=1
TEST_PAGE_1_OFFSET=3


function log() {
	echo -e "## log: $@"
}

function cleanup() {
	echo "Cleaning up"
	
	if [ "x${BLOCK_ERASED_PATH}" != "x" ]; then
		rm -f ${BLOCK_ERASED_PATH}
	fi
	
	if [ "x${PAGE_ERASED_PATH}" != "x" ]; then
		rm -f ${PAGE_ERASED_PATH}
	fi
	
	if [ "x${TMP_PATH}" != "x" ]; then
		rm -f ${TMP_PATH}
	fi

	if [ "x${PAGE_TEST_PATH}" != "x" ]; then
		rm -f ${PAGE_TEST_PATH}
	fi
	
	if [ "x${BLOCK_TEST_PATH}" != "x" ]; then
		rm -f ${BLOCK_TEST_PATH}
	fi	
}

FLASH_UTIL_READER_PARAM="-s ${SERIAL_PORT} --serial-baud ${SERIAL_BAUD}"

function callFlashUtil() {
	local cmd="${FLASH_UTIL} ${FLASH_UTIL_READER_PARAM} $@"

	echo "calling \"$cmd\"" 1>&2
	
	eval "$cmd 2>&1"
}

UTIL_OUT=$(callFlashUtil)

FLASH_PAGE_SIZE=256
FLASH_SIZE=$(echo "${UTIL_OUT}" | sed -n "s/^.*size: \([0-9]\+\)B.*$/\1/p")
FLASH_BLOCKS=$(echo "${UTIL_OUT}" | sed -n "s/^.*blocks: \([0-9]\+\) .*$/\1/p")
FLASH_SECTORS=$(echo "${UTIL_OUT}" | sed -n "s/^.*sectors: \([0-9]\+\) .*$/\1/p")
FLASH_PAGES=$(( ${FLASH_SIZE} / ${FLASH_PAGE_SIZE} ))
FLASH_BLOCK_SIZE=$(( ${FLASH_SIZE} / ${FLASH_BLOCKS} ))
FLASH_SECTOR_SIZE=$(( ${FLASH_SIZE} / ${FLASH_SECTORS} ))

FLASH_SECTORS_PER_BLOCK=$(( ${FLASH_SECTORS} / ${FLASH_BLOCKS} ))
FLASH_PAGES_PER_SECTOR=$(( ${FLASH_SECTOR_SIZE} / ${FLASH_PAGE_SIZE} ))


echo "##"
echo "## FLASH_SIZE:       ${FLASH_SIZE} Bytes"
echo "## FLASH_BLOCKS:     ${FLASH_BLOCKS}"
echo "## FLASH_SECTORS:    ${FLASH_SECTORS}"
echo "## FLASH_PAGES:      ${FLASH_PAGES}"
echo "## BLOCK_SIZE:       ${FLASH_BLOCK_SIZE}"
echo "## SECTOR_SIZE:      ${FLASH_SECTOR_SIZE}"
echo "## PAGE_SIZE:        ${FLASH_PAGE_SIZE}"
echo "##"
echo "## SECTORS_PER_BLOCK ${FLASH_SECTORS_PER_BLOCK}"
echo "## PAGES_PER_SECTOR  ${FLASH_PAGES_PER_SECTOR}"
echo "##"

trap cleanup EXIT

BLOCK_ERASED_PATH=$(mktemp)
PAGE_ERASED_PATH=$(mktemp)
PAGE_TEST_PATH=$(mktemp)
BLOCK_TEST_PATH=$(mktemp)

TMP_PATH=$(mktemp)

head -c ${FLASH_BLOCK_SIZE} /dev/zero | tr "\000" "\377" > ${BLOCK_ERASED_PATH}
cp ${BLOCK_ERASED_PATH} ${BLOCK_TEST_PATH}

head -c ${FLASH_PAGE_SIZE}  /dev/zero | tr "\000" "\377" > ${PAGE_ERASED_PATH}
head -c ${FLASH_PAGE_SIZE}  /dev/urandom > ${PAGE_TEST_PATH}

log "Erasing test block"
callFlashUtil --erase-block ${TEST_BLOCK_IDX} --no-redudant-cycles

log "Writing test page"
cat ${PAGE_TEST_PATH} | dd of=${BLOCK_TEST_PATH} bs=1 conv=notrunc seek=$(( ${TEST_PAGE_0_OFFSET} * ${FLASH_PAGE_SIZE} ))
cat ${PAGE_TEST_PATH} | dd of=${BLOCK_TEST_PATH} bs=1 conv=notrunc seek=$(( ${TEST_PAGE_1_OFFSET} * ${FLASH_PAGE_SIZE} ))

callFlashUtil --write-block ${TEST_BLOCK_IDX} -i ${BLOCK_TEST_PATH}

log "Verifying test block"
callFlashUtil --read-block ${TEST_BLOCK_IDX} -o ${TMP_PATH}
diff ${TMP_PATH} ${BLOCK_TEST_PATH}

hexdump -C ${BLOCK_TEST_PATH}
hexdump -C ${TMP_PATH}

log "Testing erase sector"
callFlashUtil --erase-sector $(( ${TEST_BLOCK_IDX} * ${FLASH_SECTORS_PER_BLOCK} )) --no-redudant-cycles
callFlashUtil --read-block ${TEST_BLOCK_IDX} -o ${TMP_PATH}


log "SUCCESS"
