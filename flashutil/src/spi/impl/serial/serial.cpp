#include <memory>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <boost/asio.hpp>
#define BOOST_BIND_GLOBAL_PLACEHOLDERS
#include <boost/bind.hpp>

#include "exception.h"

#include "serial.h"
#include "debug.h"


enum class Result {
	IN_PROGRESS,
	SUCCESS,
	ERROR,
	TIMEOUT
};


struct Serial::Impl {
	boost::asio::io_service  service;
	boost::asio::serial_port serial;

	boost::asio::deadline_timer timeoutTimer;
	Result                      readResult;

	void onCompleted(const boost::system::error_code &errorCode, const size_t bytesTransferred) {
		DBG(("CALL, read: %zd bytes (%s)", bytesTransferred, errorCode.message().c_str()));

		if (errorCode) {
			if (errorCode != boost::asio::error::operation_aborted) {
				this->timeoutTimer.cancel();

				this->readResult = Result::ERROR;
			}

		} else {
			this->readResult = Result::SUCCESS;

			this->timeoutTimer.cancel();
		}
	}

	void onTimedOut(const boost::system::error_code &errorCode) {
		if (errorCode != boost::asio::error::operation_aborted) {
			this->serial.cancel();

			this->readResult = Result::TIMEOUT;
		}
	}

	Impl(const std::string &serialPort) : service(), serial(service, serialPort), timeoutTimer(service) {
		this->readResult = Result::SUCCESS;
	}
};


Serial::Serial(const std::string &serialPath, int baud) {
	this->self.reset(new Impl(serialPath));

	PRINTFLN(("Device %s opened!", serialPath.c_str()));

	{
		boost::asio::serial_port &s = self->serial;

		s.set_option(boost::asio::serial_port::baud_rate(baud));
		s.set_option(boost::asio::serial_port::character_size(8));
		s.set_option(boost::asio::serial_port::flow_control(boost::asio::serial_port::flow_control::none));
		s.set_option(boost::asio::serial_port::parity(boost::asio::serial_port::parity::none));
		s.set_option(boost::asio::serial_port::stop_bits(boost::asio::serial_port::stop_bits::one));
	}

#if defined(__unix__)
	{
		int fd = self->serial.lowest_layer().native_handle();

		struct termios options;

		if (tcgetattr(fd, &options) != 0) {
			throw_Exception("Unable to get serial port options!");
		}

		options.c_cflag &= ~HUPCL; // Lower modem control lines after last process closes the device - prevent form arduino reset on next call

		// raw transmission
		options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
		options.c_oflag &= ~OPOST;

		options.c_cc[VMIN] = 1;

		if (tcsetattr(fd, TCSANOW, &options) != 0) {
			throw_Exception("Unable to set serial port options!");
		}

		tcflush(fd, TCIOFLUSH);
	}
#endif
}


Serial::~Serial() {

}


void Serial::write(void *buffer, std::size_t bufferSize, int timeoutMs) {
	if (self->serial.write_some(boost::asio::buffer(buffer, bufferSize)) != bufferSize) {
		throw_Exception("Cannot write data to the output!");
	}
}


void Serial::read(void *buffer, std::size_t bufferSize, int timeoutMs) {
	self->service.restart();

	if (timeoutMs != 0) {
		self->timeoutTimer.expires_from_now(boost::posix_time::milliseconds(timeoutMs));

	} else {
		self->timeoutTimer.expires_from_now(boost::posix_time::hours(100000));
	}

	self->timeoutTimer.async_wait(
		boost::bind(
			&Impl::onTimedOut,
			self.get(),
			boost::asio::placeholders::error
		)
	);

	self->readResult = Result::IN_PROGRESS;

	boost::asio::async_read(
		self->serial,
		boost::asio::buffer(buffer, bufferSize),
		boost::bind(
			&Impl::onCompleted,
			self.get(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred
		)
	);

	self->service.run();

	switch (self->readResult) {
		case Result::IN_PROGRESS:
			break;

		case Result::ERROR:
			{
				self->timeoutTimer.cancel();
				self->serial.cancel();

				throw_Exception("Error occurred while reading data from serial port!");
			}
			break;

		case Result::SUCCESS:
			{
				DBG(("RESULT_SUCCESS"));

				self->timeoutTimer.cancel();
			}
			break;

		case Result::TIMEOUT:
			{
				DBG(("RESULT_TIMEOUT"));

				self->serial.cancel();

				throw_Exception("Timeout occurred while waiting on data");
			}
			break;
	}
}
