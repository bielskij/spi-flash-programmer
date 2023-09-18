/*
 * spi.h
 *
 *  Created on: 6 mar 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef SPI_H_
#define SPI_H_

#include <set>
#include <map>
#include <vector>
#include <cinttypes>

class Spi {
	public:
		class Message {
			public:
				class Flags {
					public:
						Flags() {
							this->reset();
						}

						Flags &chipDeselect(bool deselect) {
							this->_chipDeselect = deselect;  return *this;
						}

						Flags &reset() {
							this->chipDeselect(true); return *this;
						}

					private:
						bool _chipDeselect;
				};

				class SendOpts {
					public:
						SendOpts &byte(uint8_t byte);
						SendOpts &data(uint8_t *data, std::size_t dataSize);
						SendOpts &dummy();

						std::size_t getBytes() const;
						const uint8_t *data() const;

						SendOpts &reset();

					private:
						std::vector<uint8_t> _data;
				};

				class RecvOpts {
					public:
						RecvOpts &skip(std::size_t count = 0);
						RecvOpts &bytes(std::size_t count = 0);

						std::size_t getSkips() const;
						std::size_t getBytes() const;
						std::set<std::size_t> getSkipMap() const;

						uint8_t *data();
						uint8_t at(std::size_t pos) const;

						RecvOpts &reset();

					private:
						std::vector<uint8_t>               _data;
						std::map<std::size_t, std::size_t> _skips;
				};

			public:
				Message();
				virtual ~Message();

			public:
				SendOpts &send();
				RecvOpts &recv();
				Flags    &flags();

				Message &reset();

			private:
				Flags    _flags;
				SendOpts _sendOpts;
				RecvOpts _recvOpts;
		};

		class Messages {
			public:
				Messages();

				Message &add();
				Message &at(std::size_t pos);
				std::size_t count() const;

			private:
				std::vector<Message> _msgs;
		};

		class Config {
			public:
				Config() {
				}
		};

		class Capabilities {
			public:
				Capabilities() {
					this->_transferSizeMax = 0;
				}

				std::size_t getTransferSizeMax() {
					return this->_transferSizeMax;
				}

				void setTransferSizeMax(std::size_t size) {
					this->_transferSizeMax = size;
				}

			private:
				std::size_t _transferSizeMax;
		};

	public:
		virtual ~Spi() {}

		virtual void transfer(Messages &msgs) = 0;
		virtual void chipSelect(bool select) = 0;

		virtual Config getConfig() = 0;
		virtual void   setConfig(const Config &config) = 0;

		virtual const Capabilities &getCapabilities() const = 0;
		virtual void attach() = 0;
		virtual void detach() = 0;

	protected:
		Spi() {}
};

#endif /* SPI_H_ */
