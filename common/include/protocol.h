/*
 * protocol.h
 *
 *  Created on: 27 lut 2022
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef FIRMWARE_INCLUDE_PROTOCOL_H_
#define FIRMWARE_INCLUDE_PROTOCOL_H_

#define PROTO_SYNC_BYTE  0x3d

#define PROTO_CRC8_POLY  0xAB
#define PROTO_CRC8_START 0x00

#define PROTO_NO_ERROR                0x00
#define PROTO_ERROR_INVALID_SYNC_BYTE 0x01
#define PROTO_ERROR_INVALID_CMD       0x02
#define PROTO_ERROR_TIMEOUT           0x03
#define PROTO_ERROR_INVALID_LENGTH    0x04
#define PROTO_ERROR_INVALID_CRC       0x05


#define PROTO_CMD_SPI_CS_HI    0x01
#define PROTO_CMD_SPI_CS_LO    0x02
#define PROTO_CMD_SPI_TRANSFER 0x03


#endif /* FIRMWARE_INCLUDE_PROTOCOL_H_ */
