/*
 * spi.h
 *
 *  Created on: 6 mar 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef SPI_H_
#define SPI_H_

#include <vector>
#include <cinttypes>

class Spi {
	public:
		class Message {
			public:
				class SendOpts {
					public:
						SendOpts &byte(uint8_t byte);
						SendOpts &data(uint8_t *data, size_t dataSize);
						SendOpts &dummy();
				};

				class RecvOpts {
					public:
						RecvOpts &skip(size_t count = 0);
						RecvOpts &byte(size_t count = 0);
				};

			public:
				Message(size_t size);
				virtual ~Message();

			public:
				SendOpts &send();
				RecvOpts &recv();

				uint8_t  at(size_t pos) const;
				uint8_t *data();
				size_t   size() const;
				const uint8_t *data() const;

			private:
				std::vector<uint8_t> _data;
		};

		class Messages {
			public:
				Messages();

				Message &add();
				const Message &at(size_t pos) const;
				size_t   count() const;

			private:
				std::vector<Message> _msgs;
		};

	public:
		virtual ~Spi() {}

		virtual void transfer(const Messages &msgs) = 0;

	protected:
		Spi() {}

		Message &addMessage(size_t size, bool );
};

#endif /* SPI_H_ */
