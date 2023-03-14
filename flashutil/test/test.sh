#!/usr/bin/env bash

set -e

SERIAL_PORT=/dev/ttyUSB0
SERIAL_BAUD=1000000

FLASH_UTIL="${FLASH_UTIL:-../build/flash-util}"

PAGE_SIZE=256

TEST_BLOCK_IDX=2
TEST_PAGE_OFFSET=2


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

FLASH_UTIL_READER_PARAM="-p ${SERIAL_PORT} -b ${SERIAL_BAUD}"

UTIL_OUT=$(${FLASH_UTIL} ${FLASH_UTIL_READER_PARAM}  2>&1)

FLASH_BLOCKS=$(echo "${UTIL_OUT}" | sed -n "s/^.*blocks: \([0-9]\+\) .*$/\1/p")
FLASH_PAGES=$(echo "${UTIL_OUT}" | sed -n "s/^.*sectors: \([0-9]\+\) .*$/\1/p")
FLASH_SIZE=$(echo "${UTIL_OUT}" | sed -n "s/^.*size: \([0-9]\+\)B.*$/\1/p")
FLASH_PAGES_PER_BLOCK=$(( ${FLASH_PAGES} / ${FLASH_BLOCKS} ))
FLASH_BLOCK_SIZE=$(( ${FLASH_SIZE} / ${FLASH_BLOCKS} ))
FLASH_PAGE_SIZE=$(( ${FLASH_SIZE} / ${FLASH_PAGES} ))

echo "${FLASH_BLOCKS} ${FLASH_PAGES} ${FLASH_PAGES_PER_BLOCK} ${FLASH_SIZE} ${FLASH_BLOCK_SIZE} ${FLASH_PAGE_SIZE}"

trap cleanup EXIT

BLOCK_ERASED_PATH=$(mktemp)
PAGE_ERASED_PATH=$(mktemp)
PAGE_TEST_PATH=$(mktemp)
BLOCK_TEST_PATH=$(mktemp)

TMP_PATH=$(mktemp)

head -c ${FLASH_BLOCK_SIZE} /dev/zero | tr "\000" "\377" > ${BLOCK_ERASED_PATH}
head -c ${FLASH_BLOCK_SIZE} /dev/urandom > ${BLOCK_TEST_PATH}

head -c ${FLASH_PAGE_SIZE}  /dev/zero | tr "\000" "\377" > ${PAGE_ERASED_PATH}
head -c ${FLASH_PAGE_SIZE}  /dev/urandom > ${PAGE_TEST_PATH}

log "Checking if test block is erased"

${FLASH_UTIL} ${FLASH_UTIL_READER_PARAM} -r ${TEST_BLOCK_IDX} -o ${TMP_PATH}

if diff ${TMP_PATH} ${BLOCK_ERASED_PATH}; then
	log "Test block is already erased"
else
	log "Erasing test block"
	
	${FLASH_UTIL} ${FLASH_UTIL_READER_PARAM} -e ${TEST_BLOCK_IDX}
fi

log "Writing test page"

#${FLASH_UTIL} ${FLASH_UTIL_READER_PARAM} 

log "SUCCESS"
