/*
 * spi.cpp
 *
 *  Created on: 6 mar 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

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


Spi::Message::RecvOpts &Spi::Message::RecvOpts::skip(std::size_t count) {
	return *this;
}


Spi::Message::RecvOpts &Spi::Message::RecvOpts::byte(std::size_t count) {
	this->_data.push_back(0xff);

	return *this;
}


std::size_t Spi::Message::RecvOpts::getSkips() const {
	std::size_t ret = 0;

	for (auto &p : this->_skips) {
		ret += p.second;
	}

	return ret;
}


std::size_t Spi::Message::RecvOpts::getBytes() const {
	return this->_data.size();
}


uint8_t *Spi::Message::RecvOpts::data() {
	return this->_data.data();
}


const uint8_t *Spi::Message::RecvOpts::data() const {
	return this->_data.data();
}


uint8_t Spi::Message::RecvOpts::at(std::size_t pos) const {
	return this->_data.at(pos);
}


Spi::Message::Message() {

}


Spi::Message::~Message() {

}


Spi::Message::SendOpts &Spi::Message::send() {
	return this->_sendOpts;
}


Spi::Message::RecvOpts &Spi::Message::recv() {
	return this->_recvOpts;
}


const Spi::Message::SendOpts &Spi::Message::send() const {
	return this->_sendOpts;
}


const Spi::Message::RecvOpts &Spi::Message::recv() const {
	return this->_recvOpts;
}


Spi::Messages::Messages() {

}


Spi::Message &Spi::Messages::add() {
	this->_msgs.push_back(Spi::Message());

	return *this->_msgs.rbegin();
}


const Spi::Message &Spi::Messages::at(std::size_t pos) const {
	return this->_msgs.at(pos);
}


std::size_t Spi::Messages::count() const {
	return this->_msgs.size();
}
