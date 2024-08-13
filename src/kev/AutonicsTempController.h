#pragma once

#include <RS485.h>
#include <errno.h>
#include <libmodbus/modbus-rtu.h>

#include "kev/Log.h"
#include "libmodbus/modbus.h"

namespace kev {

constexpr auto AUTONICS_SPEED = 9600;
constexpr auto AUTONICS_SERIAL_CONFIG = SERIAL_8N1;
constexpr auto AUTONICS_MODBUS_TIMEOUT_US = 30000;       // 30ms
constexpr auto AUTONICS_MODBUS_BYTE_TIMEOUT_US = 30000;  // 30ms
constexpr auto AUTONICS_SV_ADDRESS = 0x0000;             // holding reg
constexpr auto AUTONICS_RUN_ADDRESS = 0x0032;            // holding reg
constexpr auto AUTONICS_PV_ADDRESS = 0x03E8;             // input reg
constexpr auto AUTONICS_OUT1_ADDRESS = 0x0003;           // input bit

template <typename = void>
struct AutonicsTempControllerImpl {
	AutonicsTempControllerImpl(RS485Class& rs485, uint8_t address) {
		mb = modbus_new_rtu(&rs485, AUTONICS_SPEED, AUTONICS_SERIAL_CONFIG);
		modbus_set_slave(mb, address);
		modbus_set_response_timeout(mb, 0, AUTONICS_MODBUS_TIMEOUT_US);
		modbus_set_byte_timeout(mb, 0, AUTONICS_MODBUS_BYTE_TIMEOUT_US);
	}

	auto begin() -> void { modbus_connect(mb); }

	auto setSv(int sv) -> void {
		modbus_write_register(mb, AUTONICS_SV_ADDRESS, sv);
		if (errno) {
			log_("Failed to set SV: ", strerror(errno));
		}
	}

	auto readPv() -> std::optional<int> {
		uint16_t pv;
		modbus_read_input_registers(mb, AUTONICS_PV_ADDRESS, 1, &pv);
		if (errno) {
			log_("Failed to read PV: ", strerror(errno));
			return std::nullopt;
		}
		return static_cast<int>(pv);
	}

	auto readOut1() -> std::optional<bool> {
		uint8_t out1;
		modbus_read_input_bits(mb, AUTONICS_OUT1_ADDRESS, 1, &out1);
		if (errno) {
			log_("Failed to read output: ", strerror(errno));
			return std::nullopt;
		}
		return out1;
	}

	auto setRun(bool run) -> void {
		auto const value = run ? 0 : 1;  // 0: run, 1: stop
		modbus_write_register(mb, AUTONICS_RUN_ADDRESS, value);
		if (errno) {
			log_("Failed to set run: ", strerror(errno));
		}
	}

   private:
	modbus_t* mb;
	Log<> log_{"AutonicsTempController"};
};

using AutonicsTempController = AutonicsTempControllerImpl<>;

}  // namespace kev
