/*
 * protocol.h
 *
 *  Created on: 27 lut 2022
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef FIRMWARE_INCLUDE_PROTOCOL_H_
#define FIRMWARE_INCLUDE_PROTOCOL_H_

#define PROTO_VERSION_MAJOR 1
#define PROTO_VERSION_MINOR 0

/*
 * CRC8 start byte and polynomial definition.
 */
#define PROTO_CRC8_POLY  0xAB
#define PROTO_CRC8_START 0x00

#define PROTO_SYNC_NIBBLE 0xd0

/*
 * VLEN field definition.
 *
 * VLEN is a variable length field containing unsigned integer value. It is:
 *  - 1B length if the MSB bit is cleared. Allowed integer range is 0 - 127
 *  - 2B length if the MSB bit is set. Allowed integer range is 0 - 32767
 */

/*
 * 1) Protocol frame.
 *
 * [      8b      ][ 1/2B ][         LEN        ]
 * [  4b  ][  4b  ][      ][      ][    ][      ]
 * [ SYNC ][ CTRL ][ VLEN ][ PLD  ][... ][ CRC8 ]
 *
 * SYNC: Synchronization byte. Always 0xd0.
 * CTRL: [R...] command (request) or error code (response) MSB bit is reserved for future use. Should be set to 0.
 * VLEN: Length of command data (includes payload length and last byte containing CRC checksum. Does not include sync+cmd byte).
 * PLD:  frame payload
 * CRC8: checksum of overall frame
 */

/*
 * 2) CMD_GET_INFO.
 *
 * This commands returns the following information:
 *  - protocol version
 *  - HW name
 *  - maximal payload size
 *
 * [  1B  ][    1B   ][    1B   ][    1B    ][  NAME_LEN   ][   1/2B   ]
 * [ 0xd0 ][ VER_MAJ ][ VER_MIN ][ NAME_LEN ][ NAME ][ ... ][ PLD_SIZE ]
 */
#define PROTO_CMD_GET_INFO     0x00

/*
 * 3) CMD_SPI_TRANSFER
 *
 * Do SPI data transfer.
 *
 * [ 1B ][ 1B  ][     1/2B    ][  1/2B   ][     1/2B    ][  1/2B   ][     TX_SIZE    ][     RX_SIZE    ]
 * [0xd1][FLAGS][ TX_SKP_VLEN ][ TX_VLEN ][ RX_SKP_VLEN ][ RX_VLEN ][ TX_DATA ][ ... ][ RX_DATA ][ ... ]
 *
 * Flags:
 *  0000 0001 - has TX (TX_SKP_VLEN, TX_VLEN, TX_DATA omitted if the flag is not set)
 *  0000 0010 - has RX (RX_SKP_VLEN, RX_VLEN, RX_DATA omitted if the flag is not set)
 *  0000 0100 - Assert CS at the beginning
 *  0000 1000 - Release CS at the end
 */
#define PROTO_CMD_SPI_TRANSFER 0x01

/*
 * Error codes.
 */
#define PROTO_NO_ERROR                0x00
#define PROTO_ERROR_INVALID_SYNC_BYTE 0x01
#define PROTO_ERROR_INVALID_CMD       0x02
#define PROTO_ERROR_TIMEOUT           0x03
#define PROTO_ERROR_INVALID_LENGTH    0x04
#define PROTO_ERROR_INVALID_CRC       0x05


#endif /* FIRMWARE_INCLUDE_PROTOCOL_H_ */
