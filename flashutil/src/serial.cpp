#include <memory>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include "serial.h"
#include "debug.h"


struct SerialImpl {
	Serial base;

	enum class Result {
		IN_PROGRESS,
		SUCCESS,
		ERROR,
		TIMEOUT
	};

	boost::asio::io_service  service;
	boost::asio::serial_port serial;

	boost::asio::deadline_timer timeoutTimer;
	boost::asio::streambuf      readBuffer;
	Result                      readResult;

	SerialImpl(const std::string &serialPort) : service(), serial(service, serialPort), timeoutTimer(service) {
		this->readResult = Result::SUCCESS;
	}

	void onCompleted(const boost::system::error_code &errorCode, const size_t bytesTransferred) {
		DBG(("CALL, read: %zd bytes", bytesTransferred));

		if (errorCode) {
			this->readResult = Result::ERROR;

		} else {
			this->readResult = Result::SUCCESS;
		}
	}

	void onTimedOut(const boost::system::error_code &errorCode) {
		if (! errorCode && this->readResult == Result::IN_PROGRESS) {
			this->readResult = Result::TIMEOUT;
		}
	}
};


static bool _opWrite(struct _Serial *self, void *buffer, size_t bufferSize, int timeoutMs) {
	SerialImpl *s = reinterpret_cast<SerialImpl *>(self);

	DBG(("CALL, ptr: %p, size: %zd", buffer, bufferSize));

	if (s->serial.write_some(boost::asio::buffer(buffer, bufferSize)) != bufferSize) {
		ERR(("Cannot write data to the output!"));

		return false;
	}

	return true;
}


static bool _opReadByte(struct _Serial *self, uint8_t *value, int timeoutMs) {
	SerialImpl *s = reinterpret_cast<SerialImpl *>(self);

	DBG(("CALL ptr: %p, timeout %d", value, timeoutMs));

	if(s->readBuffer.size() > 0) {
		std::istream is(&s->readBuffer);

		is.read((char *) value, 1);

	} else {
		s->readResult = SerialImpl::Result::IN_PROGRESS;

		boost::asio::async_read(
			s->serial,
			boost::asio::buffer(value, 1),
			boost::bind(
				&SerialImpl::onCompleted,
				s,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred
			)
		);

		if (timeoutMs != 0) {
			s->timeoutTimer.expires_from_now(boost::posix_time::milliseconds(timeoutMs));

		} else {
			s->timeoutTimer.expires_from_now(boost::posix_time::hours(100000));
		}

		s->timeoutTimer.async_wait(
			boost::bind(
				&SerialImpl::onTimedOut,
				s,
				boost::asio::placeholders::error
			)
		);

		do {
			s->service.run_one();

			switch (s->readResult) {
			case SerialImpl::Result::IN_PROGRESS:
					break;

				case SerialImpl::Result::ERROR:
					{
						DBG(("RESULT_ERROR"));

						s->timeoutTimer.cancel();
						s->serial.cancel();

						return false;
					}
					break;

				case SerialImpl::Result::SUCCESS:
					{
						std::istream is(&s->readBuffer);

						DBG(("RESULT_SUCCESS"));

						s->timeoutTimer.cancel();

						is.ignore(1);
					}
					break;

				case SerialImpl::Result::TIMEOUT:
					{
						DBG(("RESULT_TIMEOUT"));

						s->serial.cancel();
					}
					break;
			}
		} while (s->readResult == SerialImpl::Result::IN_PROGRESS);
	}

	return s->readResult == SerialImpl::Result::SUCCESS;
}


Serial *new_serial(const char *serialPath, int baud) {
	std::unique_ptr<SerialImpl> ret;

	try {
		ret.reset(new SerialImpl(serialPath));

		PRINTF(("Device %s opened!", serialPath));

		{
			boost::asio::serial_port &s = ret->serial;

			s.set_option(boost::asio::serial_port::baud_rate(baud));
			s.set_option(boost::asio::serial_port::character_size(8));
			s.set_option(boost::asio::serial_port::flow_control(boost::asio::serial_port::flow_control::none));
			s.set_option(boost::asio::serial_port::parity(boost::asio::serial_port::parity::none));
			s.set_option(boost::asio::serial_port::stop_bits(boost::asio::serial_port::stop_bits::one));
		}

#if defined(__unix__)
		{
			int fd = ret->serial.lowest_layer().native_handle();

			struct termios options;

			if (tcgetattr(fd, &options) != 0) {
				ERR(("Unable to get serial port options!"));

			} else {
				options.c_cflag &= ~HUPCL; // Lower modem control lines after last process closes the device - prevent form arduino reset on next call

				// raw transmission
				options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
				options.c_oflag &= ~OPOST;

				options.c_cc[VMIN] = 1;

				if (tcsetattr(fd, TCSANOW, &options) != 0) {
					ERR(("Unable to set serial port options!"));
				}
			}
		}
#endif

		ret->base.readByte = _opReadByte;
		ret->base.write    = _opWrite;

	} catch (const std::exception &ex) {
		PRINTF(("Unable to open serial port at %s! (%s)", serialPath, ex.what()));

		ret.reset();
	}

	return reinterpret_cast<Serial *>(ret.release());
}


void free_serial(Serial *serial) {
	SerialImpl *s = reinterpret_cast<SerialImpl *>(serial);

	delete s;
}
