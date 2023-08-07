#include "spi.h"


Spi::Message::SendOpts &Spi::Message::SendOpts::byte(uint8_t byte) {
	this->_data.push_back(byte);

	return *this;
}


Spi::Message::SendOpts &Spi::Message::SendOpts::data(uint8_t *data, std::size_t dataSize) {
	std::copy(data, data + dataSize, std::back_inserter(this->_data));

	return *this;
}


Spi::Message::SendOpts &Spi::Message::SendOpts::dummy() {
	this->_data.push_back(0xff);

	return *this;
}


std::size_t Spi::Message::SendOpts::getBytes() const {
	return this->_data.size();
}


const uint8_t *Spi::Message::SendOpts::data() const {
	return this->_data.data();
}


Spi::Message::SendOpts &Spi::Message::SendOpts::reset() {
	this->_data.clear();

	return *this;
}


Spi::Message::RecvOpts &Spi::Message::RecvOpts::skip(std::size_t count) {
	std::size_t pos = this->_data.size();

	for (auto &skip : this->_skips) {
		pos += skip.second;
	}

	this->_skips[pos] = count;

	return *this;
}


Spi::Message::RecvOpts &Spi::Message::RecvOpts::bytes(std::size_t count) {
	this->_data.reserve(this->_data.size() + count);

	while (count--) {
		this->_data.push_back(0xff);
	}

	return *this;
}


std::size_t Spi::Message::RecvOpts::getSkips() const {
	std::size_t ret = 0;

	for (auto &p : this->_skips) {
		ret += p.second;
	}

	return ret;
}


std::set<std::size_t> Spi::Message::RecvOpts::getSkipMap() const {
	std::set<std::size_t> ret;

	for (const auto &skip : this->_skips) {
		for (std::size_t i = 0; i < skip.second; i++) {
			ret.insert(i + skip.first);
		}
	}

	return ret;
}


std::size_t Spi::Message::RecvOpts::getBytes() const {
	return this->_data.size();
}


uint8_t *Spi::Message::RecvOpts::data() {
	return this->_data.data();
}


uint8_t Spi::Message::RecvOpts::at(std::size_t pos) const {
	return this->_data.at(pos);
}


Spi::Message::RecvOpts &Spi::Message::RecvOpts::reset() {
	this->_data.clear();
	this->_skips.clear();

	return *this;
}


Spi::Message::Message() {
	this->reset();
}


Spi::Message::~Message() {

}


Spi::Message::SendOpts &Spi::Message::send() {
	return this->_sendOpts;
}


Spi::Message::RecvOpts &Spi::Message::recv() {
	return this->_recvOpts;
}


Spi::Message::Flags &Spi::Message::flags() {
	return this->_flags;
}


Spi::Message &Spi::Message::reset() {
	this->_recvOpts.reset();
	this->_sendOpts.reset();
	this->_flags.reset();

	return *this;
}


Spi::Messages::Messages() {

}


Spi::Message &Spi::Messages::add() {
	this->_msgs.push_back(Spi::Message());

	return *this->_msgs.rbegin();
}


Spi::Message &Spi::Messages::at(std::size_t pos) {
	return this->_msgs.at(pos);
}


std::size_t Spi::Messages::count() const {
	return this->_msgs.size();
}
