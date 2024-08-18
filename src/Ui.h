#pragma once

#include <cstdio>
#include <string_view>
#include "ConfigCommon.h"
#include "HardwareSerial.h"
#include "RS485.h"
#include "State.h"
extern "C" {
#include "libmodbus/modbus-rtu.h"
#include "libmodbus/modbus.h"
}

#include "kev/Log.h"
#include "kev/Timer.h"

#include "Main.h"

using kev::Log;
using kev::Timer;
using kev::Timestamp;
using std::snprintf;
using std::string_view;

using namespace kev::literals;

constexpr auto SCREEN_BAUDS = 38400;

constexpr auto SCREEN_STATUS = 0;
constexpr auto SCREEN_CONFIG = 10;

enum class UiState {
	Status,
	Config,
};

struct Lamps {
	// Starts at address 0 of flags
	uint8_t heartbeat = false;
	uint8_t heater = false;
	uint8_t rotation = false;
	array<uint8_t, 3> fans = {false, false, false};
};

struct Buttons {
	// Starts at address 20 of flags
	uint8_t rotate_fw = false;
	uint8_t rotate_bw = false;
	uint8_t pause = false;
	uint8_t start = false;
	uint8_t preheat = false;
	uint8_t stop = false;
	uint8_t config = false;
	uint8_t config_back = false;

	auto operator==(Buttons const& other) const -> bool {
		return rotate_fw == other.rotate_fw && rotate_bw == other.rotate_bw &&
			   pause == other.pause && start == other.start &&
			   preheat == other.preheat && stop == other.stop &&
			   config == other.config;
	}

	auto operator!=(Buttons const& other) const -> bool {
		return !(*this == other);
	}
};

struct UiStage {
	uint16_t temp;
	uint16_t durationHr;
	uint16_t durationMin;

	auto operator==(UiStage const& other) const -> bool {
		return temp == other.temp && durationHr == other.durationHr &&
			   durationMin == other.durationMin;
	}

	auto operator!=(UiStage const& other) const -> bool {
		return !(*this == other);
	}
};

struct UiConfig {
	// Starts at address 20 of registers
	uint16_t preheatTemp;
	uint16_t tempHist;
	array<UiStage, 3> stages;

	auto operator==(UiConfig const& other) const -> bool {
		return preheatTemp == other.preheatTemp && tempHist == other.tempHist &&
			   stages == other.stages;
	}

	auto operator!=(UiConfig const& other) const -> bool {
		return !(*this == other);
	}
};
// Assert that it's packed as we expect
static_assert(sizeof(UiConfig) == (2 * 2 + 2 * 3 * 3));

using StrSend = array<uint16_t, 20>;

struct UiStrings {
	StrSend state;               // 100 - 120
	array<StrSend, 3> fanTemps;  // 120 - 180
	StrSend time;                // 180 - 200
	StrSend heaterTemp;          // 200 - 220
};

template <typename = void>
struct UiImpl {
	UiImpl(Main& main, State& persistent, int addr)
		: main{main}, persistent{persistent}, addr{addr} {}

	auto begin() -> void {
		mb = modbus_new_rtu(&RS485, SCREEN_BAUDS, SERIAL_8N1);
		modbus_connect(mb);
		mb_perror();

		modbus_set_slave(mb, addr);
		// modbus_set_debug(mb, true);

		sendGotoScreen(SCREEN_STATUS);
	}

	auto tick(Timestamp now) -> void {
		processCurrentState(now);

		if (inputUpdate.isDone(now)) {
			inputUpdate.reset(now);

			auto start2 = millis();
			processInput(now);
			auto end2 = millis();
			avgInput = avgInput * 0.9 + (end2 - start2) * 0.1;
		}
		auto end = Timestamp{millis()};
	}

   private:
	auto processCurrentState(Timestamp now) -> void {
		if (stateUpdate.isDone(now)) {
			auto start1 = millis();
			updateScreen(now);
			auto end1 = millis();
			avgCurrState = avgCurrState * 0.9 + (end1 - start1) * 0.1;
			stateUpdate.reset(millis());
		}
	}

	auto processInput(Timestamp now) -> void {
		auto buttons = Buttons{};
		delay(1);
		modbus_read_bits(mb, 20, sizeof(buttons),
						 reinterpret_cast<uint8_t*>(&buttons));

		if (buttons.preheat && !prevButtons.preheat) {
			main.eventUiPreheat(now);
		}

		if (buttons.stop && !prevButtons.stop) {
			main.eventUiStop(now);
		}

		if (buttons.start && !prevButtons.start) {
			main.eventUiStart(now);
		}

		if (buttons.config && !prevButtons.config) {
			sendGotoScreen(SCREEN_CONFIG);
			sendConfigScreen();
			state = UiState::Config;
		}

		if (buttons.rotate_fw && !prevButtons.rotate_fw) {
			main.eventUiRotateFw();
		}

		if (buttons.rotate_bw && !prevButtons.rotate_bw) {
			main.eventUiRotateBw();
		}

		if (!buttons.rotate_fw && prevButtons.rotate_fw) {
			main.eventUiRotateFwStop();
		}

		if (!buttons.rotate_bw && prevButtons.rotate_bw) {
			main.eventUiRotateBwStop();
		}

		if (buttons.pause && !prevButtons.pause) {
			main.eventUiPause(now);
		}

		if (buttons.config_back && !prevButtons.config_back) {
			sendGotoScreen(SCREEN_STATUS);
			sendConfigScreen();
			state = UiState::Status;
		}

		if (buttons != prevButtons) {
			updateScreen(now);
		}

		if (state == UiState::Config) {
			auto uiConfig = UiConfig{};
			delay(1);
			modbus_read_registers(mb, 20, sizeof(uiConfig) / sizeof(uint16_t),
								  reinterpret_cast<uint16_t*>(&uiConfig));

			if (uiConfig != prevUiConfig && uiConfig.preheatTemp != 0) {
				log("using new config from UI");
				auto config = configFromUiConfig(uiConfig);
				main.setConfig(config);

				persistent.inner.config = config;
				persistent.persist();
			}
			prevUiConfig = uiConfig;
		}

		prevButtons = buttons;
	}

	auto updateScreen(Timestamp now) -> void {
		delay(1);

		heartbeat = !heartbeat;
		auto s1 = millis();
		sendLamps(Lamps{
			.heartbeat = heartbeat,
			.heater = main.readHeater(now),
			.rotation = main.readRotation(),
			.fans = {main.readFan(0), main.readFan(1), main.readFan(2)},
		});
		avgSendLamps = avgSendLamps * 0.7 + (millis() - s1) * 0.3;

		auto payload = UiStrings{};
		setString(payload.state, main.displayState());

		for (auto i = 0; i < 3; ++i) {
			auto temp = array<char, 20>{};
			auto tempVal = main.readTemp(i, now);
			if (!tempVal) {
				setString(payload.fanTemps[i], "Error de sensor");
				continue;
			}
			snprintf(temp.data(), temp.size(), "%.1f °C", *tempVal);
			setString(payload.fanTemps[i], temp.data());
		}

		auto temp = array<char, 20>{};
		auto s2 = millis();
		auto tempVal = main.heaterTemp(now);
		avgReqPv = avgReqPv * 0.7 + (millis() - s2) * 0.3;
		if (!tempVal) {
			setString(payload.heaterTemp, "Error de sensor");
		} else {
			snprintf(temp.data(), temp.size(), "%.1f °C", *tempVal);
			setString(payload.heaterTemp, temp.data());
		}

		auto const timer = main.readCurrentTimer(now);
		if (timer) {
			auto time = array<char, 20>{};
			snprintf(time.data(), time.size(), "%02d:%02d:%02d",
					 timer->unsafeGetValue() / 1000 / 60 / 60,
					 timer->unsafeGetValue() / 1000 / 60 % 60,
					 timer->unsafeGetValue() / 1000 % 60);
			setString(payload.time, time.data());
		} else {
			setString(payload.time, "N/A");
		}

		auto s3 = millis();
		modbus_write_registers(mb, 100, sizeof(UiStrings) / sizeof(uint16_t),
							   reinterpret_cast<uint16_t*>(&payload));
		avgSendStrings = avgSendStrings * 0.7 + (millis() - s3) * 0.3;
		mb_perror();
	}

	auto setString(StrSend& target, std::string_view str) -> void {
		auto const maxLen = target.size();

		for (auto i = 0u, j = 0u; j < str.size() && i < maxLen; ++i, j += 2) {
			target[i] = static_cast<unsigned char>(str[j]);
			if ((j + 1) < str.size()) {
				target[i] |= static_cast<unsigned char>(str[j + 1]) << 8;
			}
		}
	}

	auto sendLamps(Lamps lamps) {
		modbus_write_bits(mb, 0, sizeof(lamps),
						  reinterpret_cast<uint8_t*>(&lamps));
		mb_perror();
	}

	auto mb_perror() -> bool {
		if (errno != 0) {
			log("modbus error: ", modbus_strerror(errno));
			errno = 0;
			return true;
		}
		return false;
	}

	auto sendGotoScreen(int screen) -> void {
		auto const screen_reg = static_cast<uint16_t>(screen);
		delay(1);
		modbus_write_registers(mb, 0, 1, &screen_reg);
		mb_perror();
	}

	auto sendConfigScreen() -> void {
		auto uiConfig = uiConfigFromConfig(main.getConfig());
		delay(1);
		modbus_write_registers(mb, 20, sizeof(uiConfig) / sizeof(uint16_t),
							   reinterpret_cast<uint16_t*>(&uiConfig));
		mb_perror();
	}

	auto configFromUiConfig(UiConfig const& uiConfig) -> Config {
		return Config{
			.preheatTemp = static_cast<double>(uiConfig.preheatTemp),
			.chamberTempHist = static_cast<double>(uiConfig.tempHist) / 10,
			.stages =
				{
					StageConfig{
						.temp = static_cast<double>(uiConfig.stages[0].temp),
						.duration = uiDuration(uiConfig.stages[0].durationHr,
											   uiConfig.stages[0].durationMin)},
					StageConfig{
						.temp = static_cast<double>(uiConfig.stages[1].temp),
						.duration = uiDuration(uiConfig.stages[1].durationHr,
											   uiConfig.stages[1].durationMin)},
					StageConfig{
						.temp = static_cast<double>(uiConfig.stages[2].temp),
						.duration = uiDuration(uiConfig.stages[2].durationHr,
											   uiConfig.stages[2].durationMin)},
				},
		};
	}

	auto uiConfigFromConfig(Config const& config) -> UiConfig {
		return UiConfig{
			.preheatTemp = static_cast<uint16_t>(config.preheatTemp),
			.tempHist = static_cast<uint16_t>(config.chamberTempHist * 10),
			.stages =
				{
					uiStageFromStage(config.stages[0]),
					uiStageFromStage(config.stages[1]),
					uiStageFromStage(config.stages[2]),
				},
		};
	}

	auto uiStageFromStage(StageConfig const& stage) -> UiStage {
		return UiStage{
			.temp = static_cast<uint16_t>(stage.temp),
			.durationHr = static_cast<uint16_t>(
				stage.duration.unsafeGetValue() / 1000 / 60 / 60),
			.durationMin = static_cast<uint16_t>(
				stage.duration.unsafeGetValue() / 1000 / 60 % 60),
		};
	}

	auto uiDuration(uint16_t hr, uint16_t min) -> kev::Duration {
		return kev::Duration{hr * (3600 * 1000) + min * (60 * 1000)};
	}

	UiState state = UiState::Status;
	bool heartbeat = false;
	Buttons prevButtons = {};
	UiConfig prevUiConfig = {};
	Timer stateUpdate{1000_ms};
	Timer inputUpdate{10_ms};

	Log<false> log{"ui"};
	modbus_t* mb;
	Main& main;
	State& persistent;
	int addr = 0;

   public:
	double avgCurrState = 0.0;
	double avgInput = 0.0;
	double avgSendLamps = 0.0;
	double avgSendStrings = 0.0;
	double avgReqPv = 0.0;
};

using Ui = UiImpl<>;
