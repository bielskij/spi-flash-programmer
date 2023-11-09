/*
 * main.c
 *
 *  Created on: 27 lut 2022
 *      Author: Jaroslaw Bielski (bielski.j@gmail.com)
 */
#include <iostream>
#include <fstream>

#include <boost/program_options.hpp>

#include "flashutil/entryPoint.h"
#include "flashutil/flash/builder.h"
#include "flashutil/spi/serial.h"
#include "flashutil/serial/hw.h"
#include "flashutil/flash/registry.h"
#include "flashutil/flash/registry/reader/json.h"

#include "flashutil/debug.h"


namespace po = boost::program_options;

#define OPT_VERBOSE  "verbose"
#define OPT_HELP     "help"
#define OPT_SERIAL   "serial"
#define OPT_OUTPUT   "output"
#define OPT_INPUT    "input"
#define OPT_BAUD     "serial-baud"
#define OPT_REGISTRY "registry"

#define OPT_READ         "read"
#define OPT_READ_BLOCK   "read-block"
#define OPT_READ_SECTOR  "read-sector"

#define OPT_ERASE        "erase"
#define OPT_ERASE_BLOCK  "erase-block"
#define OPT_ERASE_SECTOR "erase-sector"

#define OPT_WRITE        "write"
#define OPT_WRITE_BLOCK  "write-block"
#define OPT_WRITE_SECTOR "write-sector"

#define OPT_VERIFY       "verify"
#define OPT_UNPROTECT    "unprotect"

#define OPT_FLASH_DESC         "flash-geometry"
#define OPT_OMIT_REDUNDANT_OPS "no-redudant-cycles"

#define RC_SUCCESS 0
#define RC_FAILURE 1

static void _usage(const po::options_description &opts) {
	std::cerr << opts << std::endl;

	exit(RC_FAILURE);
}


int main(int argc, char *argv[]) {
	int ret = RC_FAILURE;

	do {
		std::unique_ptr<Spi>    spi;
		std::unique_ptr<Serial> serial;

		try {
			FlashRegistry                     flashRegistry;
			Flash                             flashGeometry;
			flashutil::EntryPoint::Parameters params;

			{
				po::options_description opDesc("Program parameters");

				std::string serialPath;
				int         serialBaud = 500000;

				std::ifstream inFile;
				std::ofstream outFile;

				opDesc.add_options()
					(OPT_VERBOSE     ",v", po::value<int>()->default_value(DEBUG_LEVEL_INFO), "Verbose output (0 - 5)")

					(OPT_HELP        ",h",                                               "Print usage message")
					(OPT_SERIAL      ",s", po::value<std::string>(),                     "Serial port path")
					(OPT_OUTPUT      ",o", po::value<std::string>()->default_value("-"), "Output file path or '-' for stdout")
					(OPT_INPUT       ",i", po::value<std::string>()->default_value("-"), "Input file path or '-' for stdin")
					(OPT_READ        ",r",                                               "Read to output file")
					(OPT_WRITE       ",w",                                               "Write input file")
					(OPT_ERASE       ",e",                                               "Erase whole chip")
					(OPT_VERIFY      ",V",                                               "Verify writing process")
					(OPT_UNPROTECT   ",u",                                               "Unprotect the chip before doing any operation on it")
					(OPT_FLASH_DESC  ",g", po::value<std::string>(),                     "Custom chip geometry in format <block_size>:<block_count>:<sector_size>:<sector_count>:<unprotect-mask-hex> (example: 65536:4:4096:64:8c)")
					(OPT_REGISTRY    ",R", po::value<std::string>(),                     "Path to flash registry")
					(OPT_BAUD,             po::value<int>(),                             "Serial port baudrate")
					(OPT_READ_BLOCK,       po::value<off_t>(),                           "Read block at index")
					(OPT_READ_SECTOR,      po::value<off_t>(),                           "Read Sector at index")
					(OPT_ERASE_BLOCK,      po::value<off_t>(),                           "Erase block at index")
					(OPT_ERASE_SECTOR,     po::value<off_t>(),                           "Erase sector at index")
					(OPT_WRITE_BLOCK,      po::value<off_t>(),                           "Write block from input file")
					(OPT_WRITE_SECTOR,     po::value<off_t>(),                           "Write sector from input file")
					(OPT_OMIT_REDUNDANT_OPS,                                             "Prevent from redundant erase/write cycles");
					;

				po::variables_map vm;

				po::store(po::command_line_parser(argc, argv).options(opDesc).run(), vm);

				if (vm.count(OPT_HELP)) {
					_usage(opDesc);
				}

				if (! vm.count(OPT_SERIAL)) {
					OUT("Serial port was not provided!");
					_usage(opDesc);

				} else {
					serialPath = vm[OPT_SERIAL].as<std::string>();
				}

				{
					DebugLevel level = DEBUG_LEVEL_NONE;

					if (vm.count(OPT_VERBOSE)) {
						int intLevel = vm[OPT_VERBOSE].as<int>();

						if (intLevel > DEBUG_LEVEL_LAST) {
							level = DEBUG_LEVEL_LAST;

						} else if (intLevel < 0) {
							level = DEBUG_LEVEL_NONE;

						} else {
							level = (DebugLevel) intLevel;
						}
					}

					debug_setLevel(level);
				}

				if (vm.count(OPT_REGISTRY)) {
					auto reader = std::make_unique<FlashRegistryJsonReader>();

					{
						std::string   registryPath = vm[OPT_REGISTRY].as<std::string>();
						std::ifstream registryStream(registryPath);

						if (! registryStream.is_open()) {
							ERROR("Unable to open flash registry file! (%s)", registryPath.c_str());
							break;
						}

						reader->read(registryStream, flashRegistry);
					}
				}

				for (const auto &flash : flashRegistry.getAll()) {
					DEBUG("Chip part Id: %s", flash.getPartNumber().c_str());
					DEBUG("   Manufacturer: %s", flash.getManufacturer().c_str());
					DEBUG("   ID:           %02x, %02x, %02x", flash.getId()[0], flash.getId()[1], flash.getId()[2]);
					DEBUG("   Protect_mask: %02x", flash.getProtectMask());
					DEBUG("   Flash size:   %u", flash.getSize());
					DEBUG("   Block size:   %u", flash.getBlockSize());
					DEBUG("   Sector size:  %u", flash.getSectorSize());
					DEBUG("   Page size:    %u", flash.getPageSize());
				}

				if (vm.count(OPT_BAUD)) {
					serialBaud = vm[OPT_BAUD].as<int>();
				}

				if (vm.count(OPT_OUTPUT)) {
					auto output = vm[OPT_OUTPUT].as<std::string>();
					if (output == "-") {
						params.outStream = &std::cout;

					} else {
						outFile.open(output, std::ios::out | std::ios::trunc | std::ios::binary);
						if (! outFile.is_open()) {
							ERROR("Unable to open output file! (%s)", output.c_str());
							break;
						}

						params.outStream = &outFile;
					}
				}

				if (vm.count(OPT_INPUT)) {
					auto input = vm[OPT_INPUT].as<std::string>();
					if (input == "-") {
						params.inStream = &std::cin;

					}
					else {
						inFile.open(input, std::ios::in | std::ios::binary);
						if (! inFile.is_open()) {
							ERROR("Unable to open input file! (%s)", input.c_str());
							break;
						}

						params.inStream = &inFile;
					}
				}

				if (vm.count(OPT_FLASH_DESC)) {
					auto desc = vm[OPT_FLASH_DESC].as<std::string>();

					int blockCount;
					int blockSize;
					int sectorCount;
					int sectorSize;
					uint8_t unprotectMask;

					if (sscanf(
						desc.c_str(), "%d:%d:%d:%d:%02hhx",
						&blockSize, &blockCount,
						&sectorSize, &sectorCount,
						&unprotectMask
					) != 5
					) {
						OUT("Invalid flash-geometry syntax! (%s)", desc.c_str());

						_usage(opDesc);
					}

					{
						FlashBuilder builder;

						builder
							.setName("Custom")
							.setBlockCount(blockCount)
							.setBlockSize(blockSize)
							.setSectorCount(sectorCount)
							.setSectorSize(sectorSize)
							.setProtectMask(unprotectMask);

						flashGeometry = builder.build();
					}
				}

				if (vm.count(OPT_VERIFY)) {
					params.verify = true;
				}

				if (vm.count(OPT_OMIT_REDUNDANT_OPS)) {
					params.omitRedundantWrites = true;
				}

				std::vector<flashutil::EntryPoint::Parameters> operations;

				// Detect
				{
					params.operation = flashutil::EntryPoint::Operation::NO_OPERATION;
					params.mode      = flashutil::EntryPoint::Mode::NONE;
					params.flashInfo = &flashGeometry;

					params.afterExecution = [&flashGeometry](const flashutil::EntryPoint::Parameters &params) {
						if (flashGeometry.isValid()) {
							OUT("Flash chip: %s (%02x, %02x, %02x), size: %zdB, blocks: %zd of %zdkB, sectors: %zd of %zdB",
								flashGeometry.getPartNumber().c_str(), flashGeometry.getId()[0], flashGeometry.getId()[1], flashGeometry.getId()[2],
								flashGeometry.getSize(), flashGeometry.getBlockCount(), flashGeometry.getBlockSize() / 1024,
								flashGeometry.getSectorCount(), flashGeometry.getSectorSize()
							);

						} else {
							OUT("No flash device detected!");
						}
					};

					operations.push_back(params);

					params.afterExecution = {};
				}

				// Unlock
				if (vm.count(OPT_UNPROTECT)) {
					params.operation = flashutil::EntryPoint::Operation::UNLOCK;
					params.mode      = flashutil::EntryPoint::Mode::CHIP;

					params.beforeExecution = [](const flashutil::EntryPoint::Parameters &params) {
						OUT("Unlocking flash chip");
					};

					operations.push_back(params);
				}

				// Erase
				{
					params.operation = flashutil::EntryPoint::Operation::ERASE;

					if (vm.count(OPT_ERASE)) {
						params.mode = flashutil::EntryPoint::Mode::CHIP;

						params.beforeExecution = [](const flashutil::EntryPoint::Parameters &params) {
							OUT("Erasing whole flash chip");
						};

						operations.push_back(params);
					}

					if (vm.count(OPT_ERASE_BLOCK)) {
						params.mode  = flashutil::EntryPoint::Mode::BLOCK;
						params.index = vm[OPT_ERASE_BLOCK].as<off_t>();

						params.beforeExecution = [&flashGeometry](const flashutil::EntryPoint::Parameters &params) {
							OUT("Erasing block %u (%08x): ", params.index, params.index * flashGeometry.getBlockSize());
						};

						operations.push_back(params);
					}

					if (vm.count(OPT_ERASE_SECTOR)) {
						params.mode  = flashutil::EntryPoint::Mode::SECTOR;
						params.index = vm[OPT_ERASE_SECTOR].as<off_t>();

						params.beforeExecution = [&flashGeometry](const flashutil::EntryPoint::Parameters &params) {
							OUT("Erasing sector %u (%08x): ", params.index, params.index * flashGeometry.getSectorSize());
						};

						operations.push_back(params);
					}
				}

				// Write
				{
					params.operation = flashutil::EntryPoint::Operation::WRITE;

					if (vm.count(OPT_WRITE)) {
						params.mode = flashutil::EntryPoint::Mode::CHIP;

						params.beforeExecution = [](const flashutil::EntryPoint::Parameters &params) {
							OUT("Writing whole flash chip");
						};

						operations.push_back(params);
					}

					if (vm.count(OPT_WRITE_BLOCK)) {
						params.mode  = flashutil::EntryPoint::Mode::BLOCK;
						params.index = vm[OPT_WRITE_BLOCK].as<off_t>();

						params.beforeExecution = [&flashGeometry](const flashutil::EntryPoint::Parameters &params) {
							OUT("Writing block %u (%08x): ", params.index, params.index * flashGeometry.getBlockSize());
						};

						operations.push_back(params);
					}

					if (vm.count(OPT_WRITE_SECTOR)) {
						params.mode  = flashutil::EntryPoint::Mode::SECTOR;
						params.index = vm[OPT_WRITE_SECTOR].as<off_t>();

						params.beforeExecution = [&flashGeometry](const flashutil::EntryPoint::Parameters &params) {
							OUT("Writing sector %u (%08x): ", params.index, params.index * flashGeometry.getSectorSize());
						};

						operations.push_back(params);
					}
				}

				// Read
				{
					params.operation = flashutil::EntryPoint::Operation::READ;

					if (vm.count(OPT_READ)) {
						params.mode = flashutil::EntryPoint::Mode::CHIP;

						params.beforeExecution = [](const flashutil::EntryPoint::Parameters &params) {
							OUT("Reading whole flash chip");
						};

						operations.push_back(params);
					}

					if (vm.count(OPT_READ_BLOCK)) {
						params.mode  = flashutil::EntryPoint::Mode::BLOCK;
						params.index = vm[OPT_READ_BLOCK].as<off_t>();

						params.beforeExecution = [&flashGeometry](const flashutil::EntryPoint::Parameters &params) {
							OUT("Reading block %u (%08x): ", params.index, params.index * flashGeometry.getBlockSize());
						};

						operations.push_back(params);
					}

					if (vm.count(OPT_READ_SECTOR)) {
						params.mode  = flashutil::EntryPoint::Mode::SECTOR;
						params.index = vm[OPT_READ_SECTOR].as<off_t>();

						params.beforeExecution = [&flashGeometry](const flashutil::EntryPoint::Parameters &params) {
							OUT("Reading sector %u (%08x): ", params.index, params.index * flashGeometry.getSectorSize());
						};

						operations.push_back(params);
					}
				}

				for (const auto &operation : operations) {
					if (! operation.isValid()) {
						_usage(opDesc);
					}
				}

				if (serialPath.empty()) {
					_usage(opDesc);

				} else {
					serial = std::make_unique<HwSerial>(serialPath, serialBaud);
					spi    = std::make_unique<SerialSpi>(*serial.get());
				}

				flashutil::EntryPoint::call(*spi.get(), flashRegistry, flashGeometry, operations);
			}

			ret = RC_SUCCESS;

		} catch (const std::exception &ex) {
			OUT("!! ERROR !! Cought exception: '%s'", ex.what());
		}
	} while (0);

	return ret;
}
