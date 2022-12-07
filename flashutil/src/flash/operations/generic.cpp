/*
 * generic.cpp
 *
 *  Created on: 6 gru 2022
 *      Author: jarko
 */

#include "programmer/spi/interface.h"
#include "programmer/spi/commands.h"
#include "flash/operations/generic.h"


struct flashutil::GenericOperations::Impl {
	SpiCommands cmd;
	const FlashSpec  &spec;

	Impl(const FlashSpec &spec, flashutil::SpiInterface &iface) : spec(spec), cmd(iface) {

	}
};


flashutil::GenericOperations::GenericOperations(const FlashSpec &spec, SpiInterface &iface) {
	self = std::make_unique<Impl>(spec, iface);
}


flashutil::GenericOperations::~GenericOperations() {

}


bool flashutil::GenericOperations::getChipId(Id &id) {
	bool ret = self->spec.hasOpcode(FlashOpcode::X_9F_GET_JEDEC_ID);

	if (ret) {
		uint8_t jedecId[3];

		ret = self->cmd.getJedecId(jedecId);
		if (ret) {
			id = Id(jedecId, nullptr);
		}
	}

	return ret;
}


bool flashutil::GenericOperations::erase() {
	throw OperationNotImplementedException();
}


bool flashutil::GenericOperations::eraseBlock() {
	throw OperationNotImplementedException();
}


bool flashutil::GenericOperations::eraseSector() {
	throw OperationNotImplementedException();
}
