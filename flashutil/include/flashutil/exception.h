/*
 * flashutil/exception.h
 *
 *  Created on: 7 mar 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */

#ifndef FLASHUTIL_EXCEPTION_H_
#define FLASHUTIL_EXCEPTION_H_

#include <stdexcept>
#include <string>


class Exception : public std::runtime_error {
	public:
		Exception(const std::string &msg) : std::runtime_error(msg) {

		}

		Exception(const std::string &file, int line, const std::string &msg) :
			std::runtime_error(file + ":" + std::to_string(line) + " " + msg)
		{
		}
};

#define throw_Exception(msg) do { throw Exception(__FILE__, __LINE__, msg); } while (0);

#endif /* FLASHUTIL_EXCEPTION_H_ */
