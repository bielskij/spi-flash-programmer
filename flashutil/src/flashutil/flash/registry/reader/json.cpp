#include <nlohmann/json.hpp>

#include "flashutil/flash/registry/reader/json.h"

using json = nlohmann::json;


//static uint32_t _parseNumber(const std::string &str) {
//
//}

static uint32_t _parseNumber(const json::const_reference &t) {
	if (t.is_number()) {
		return t.get<uint32_t>();

	} else if (t.is_string()) {
		std::string value      = t.get<std::string>();
		uint32_t    multiplier = 1;

		if (value.length() >= 3) {
			char unit = *value.rbegin();

			if (std::tolower(unit) == 'b') {
				char c = std::tolower(*value.rbegin());

				bool binary = false;

				c = std::tolower(*(value.rbegin() + 1));

				if (c == 'i') {
					c = std::tolower(*(value.rbegin() + 2));

					binary = true;
				}

				switch (c) {
					case 'k': multiplier = binary ? (1024)               : (1000);               break;
					case 'm': multiplier = binary ? (1024 * 1024)        : (1000 * 1000);        break;
					case 'g': multiplier = binary ? (1024 * 1024 * 1024) : (1000 * 1000 * 1000); break;

					default:
						throw std::runtime_error("Not supported size suffix!");
				}

				if (unit == 'b') {
					multiplier /= 8;
				}
			}
		}

		return std::stoul(value, nullptr, 0) * multiplier;

	} else {
		throw std::runtime_error("Not supported JSON value!");
	}
}


FlashRegistryJsonReader::FlashRegistryJsonReader() {
}


void FlashRegistryJsonReader::read(std::istream &stream, FlashRegistry &registry) {
	auto json = json::parse(stream);

	registry.clear();

	for (const auto &item : json.items()) {
		Flash flash;

		{
			const auto &definition = item.value();

			flash.setPartNumber  (definition["part_number"]);
			flash.setManufacturer(definition["manufacturer"]);

			{
				std::vector<uint8_t> jedecId;

				{
					const auto &jedecIdArray = definition["jedec_id"];

					for (const auto &id : jedecIdArray.items()) {
						jedecId.push_back(_parseNumber(id.value()));
					}
				}

				flash.setId(jedecId);
			}

			{
				const auto &geometry = definition["geometry"];

				flash.setBlockSize (_parseNumber(geometry["block_size"]));
				flash.setSectorSize(_parseNumber(geometry["sector_size"]));
				flash.setPageSize  (_parseNumber(geometry["page_size"]));
				flash.setSize      (_parseNumber(geometry["size"]));
			}

			flash.setProtectMask(_parseNumber(definition["unprotect_mask"]));
		}

		registry.addFlash(flash);
	}
}
