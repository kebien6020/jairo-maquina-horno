#pragma once

#include <RS485.h>
#include <errno.h>
#include <libmodbus/modbus-rtu.h>
#include <optional>

#include "kev/Log.h"
#include "kev/Timer.h"

namespace kev {

using std::optional;

constexpr auto AUTONICS_SPEED = 38400;
constexpr auto AUTONICS_SERIAL_CONFIG = SERIAL_8N1;
constexpr auto AUTONICS_MODBUS_TIMEOUT_US = 30000;       // 30ms
constexpr auto AUTONICS_MODBUS_BYTE_TIMEOUT_US = 30000;  // 30ms
constexpr auto AUTONICS_SV_ADDRESS = 0x0000;             // holding reg
constexpr auto AUTONICS_RUN_ADDRESS = 0x0032;            // holding reg
constexpr auto AUTONICS_PV_ADDRESS = 0x03E8;             // input reg
constexpr auto AUTONICS_OUT1_ADDRESS = 0x0003;           // input bit

template <typename = void>
struct AutonicsTempControllerImpl {
	AutonicsTempControllerImpl(RS485Class* rs485, uint8_t address) {
		mb = modbus_new_rtu(rs485, AUTONICS_SPEED, AUTONICS_SERIAL_CONFIG);
		modbus_set_slave(mb, address);
	}

	auto begin() -> void { modbus_connect(mb); }

	auto setSv(int sv) -> void {
		delay(1);
		modbus_write_register(mb, AUTONICS_SV_ADDRESS, sv);
		if (errno) {
			log_("Failed to set SV: ", strerror(errno));
		}
	}

	auto readPv(Timestamp now) -> std::optional<int> {
		if (pvTimer.isDone(now)) {
			pvTimer.reset(now);
			uint16_t pv;
			delay(1);
			modbus_read_input_registers(mb, AUTONICS_PV_ADDRESS, 1, &pv);
			if (errno) {
				log_("Failed to read PV: ", strerror(errno));
				return std::nullopt;
			}
			return lastPv = static_cast<int>(pv);
		}
		return lastPv;
	}

	auto readOut1(Timestamp now) -> std::optional<bool> {
		if (out1Timer.isDone(now)) {
			out1Timer.reset(now);
			uint8_t out1;
			delay(1);
			modbus_read_input_bits(mb, AUTONICS_OUT1_ADDRESS, 1, &out1);
			if (errno) {
				log_("Failed to read output: ", strerror(errno));
				return std::nullopt;
			}
			return lastOut1 = out1;
		}
		return lastOut1;
	}

	auto setRun(bool run) -> void {
		auto const value = run ? 0 : 1;  // 0: run, 1: stop
		delay(1);
		modbus_write_register(mb, AUTONICS_RUN_ADDRESS, value);
		if (errno) {
			log_("Failed to set run: ", strerror(errno));
			return;
		}

		running = run;
	}

	auto isRunning() -> bool {
		return running;
	}

   private:
	modbus_t* mb;
	Log<> log_{"AutonicsTempController"};
	Timer pvTimer{1500_ms};
	Timer out1Timer{1500_ms};
	optional<bool> lastOut1{false};
	optional<int> lastPv{0};
	bool running{false};
};

using AutonicsTempController = AutonicsTempControllerImpl<>;

}  // namespace kev
