/*
 * flash.h
 *
 *  Created on: 8 wrz 2023
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */


#ifndef FLASH_H_
#define FLASH_H_

#include <string>
#include <vector>

class Flash {
	public:
		Flash();
		Flash(const std::string &name, const std::vector<uint8_t> &jedecId, size_t blockSize, size_t nblocks, size_t sectorSize, size_t nSectors, uint8_t protectMask);

		const std::string &getPartNumber() const;
		void setPartNumber(const std::string &partNumber);

		const std::string &getManufacturer() const;
		void setManufacturer(const std::string &manufacturer);

		const std::vector<uint8_t> &getId() const;
		void setId(const std::vector<uint8_t> &id);

		size_t getBlockSize() const;
		void   setBlockSize(size_t size);

		size_t getBlockCount() const;
		void   setBlockCount(size_t count);

		size_t getSectorSize() const;
		void   setSectorSize(size_t size);

		size_t getSectorCount() const;
		void   setSectorCount(size_t count);

		size_t getPageSize() const;
		void   setPageSize(size_t size);

		size_t getPageCount() const;
		void   setPageCount(size_t count);

		uint8_t getProtectMask() const;
		void    setProtectMask(uint8_t mask);

		size_t getSize() const;
		void   setSize(size_t size);

		void setGeometry(const Flash &other);

		bool isIdValid() const;
		bool isGeometryValid() const;
		bool isValid() const;

	private:
		std::string          partNumber;
		std::string          manufacturer;
		std::vector<uint8_t> id;
		size_t               blockSize;
		size_t               blockCount;
		size_t               sectorSize;
		size_t               sectorCount;
		size_t               pageSize;
		size_t               pageCount;
		uint8_t              protectMask;
};


#endif /* FLASH_H_ */
