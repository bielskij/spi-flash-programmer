#include "common.h"


uint8_t proto_int_val_length_estimate(uint16_t val) {
	if (val > 127) {
		return 2;
	}

	return 1;
}


uint8_t proto_int_val_length_probe(uint8_t byte) {
	if (byte & 0x80) {
		return 2;
	}

	return 1;
}


uint16_t proto_int_val_decode(uint8_t val[2]) {
	return ((uint16_t)(val[0] & 0x7f) << 8) | val[1];
}


uint8_t proto_int_val_encode(uint16_t len, uint8_t val[2]) {
	uint8_t ret = proto_int_val_length_estimate(len);

	if (ret == 1) {
		val[0] = len;

	} else {
		val[0] = 0x80 | (len >> 8);
		val[1] = len & 0xff;
	}

	return ret;
}
