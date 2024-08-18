#include <Arduino.h>
#include <SPI.h>

#include <array>

#include "PhysicalUi.h"
#include "kev/AutonicsTempController.h"
#include "kev/Log.h"
#include "kev/Pin.h"
#include "kev/TempSensor.h"
#include "kev/Timer.h"

#include "Chamber.h"
#include "Main.h"
#include "Rotation.h"
#include "State.h"
#include "Ui.h"
#include "UiSerial.h"

constexpr auto version = "Version 2.1 (2024-08-17)";

using namespace kev::literals;
using kev::AutonicsTempController;
using kev::Input;
using kev::InputMode;
using kev::Invert;
using kev::Log;
using kev::Output;
using kev::RepeatedOutput;
using kev::Timer;
using kev::Timestamp;
using std::array;

using TempS = kev::TempSensorMax6675<>;

constexpr auto FAN_PIN1 = 26;
constexpr auto FAN_PIN2 = 27;
constexpr auto FAN_PIN3 = 14;

constexpr auto SENSOR_CS_1 = 32;
constexpr auto SENSOR_CS_2 = 33;
constexpr auto SENSOR_CS_3 = 25;

constexpr auto PHY_STOP_PIN = 36;
constexpr auto PHY_ROTATION_PIN = 39;

constexpr auto SCREEN_ADDR = 1;
constexpr auto TEMP_CONTROLLER_ADDR = 2;

constexpr auto STATS_ENABLED = true;

auto spi = SPIClass{VSPI};
auto chambers = array{
	Chamber{TempSensor{spi, SENSOR_CS_1}, Output{FAN_PIN1, Invert::Inverted}},
	Chamber{TempSensor{spi, SENSOR_CS_2}, Output{FAN_PIN2, Invert::Inverted}},
	Chamber{TempSensor{spi, SENSOR_CS_3}, Output{FAN_PIN3, Invert::Inverted}}};
auto rotation = Rotation{.fw = Output{2, Invert::Inverted},
						 .bw = Output{4, Invert::Inverted}};

auto stopInput = Input{PHY_STOP_PIN, Invert::Normal};
auto rotationInput = Input{PHY_ROTATION_PIN, Invert::Inverted};

auto tempController = AutonicsTempController{&RS485, TEMP_CONTROLLER_ADDR};

auto main_ = Main{chambers, rotation, tempController};

auto persistent = State{};

Log<> log_{"main"};
Timer logTimer{1_s};
UiSerial uiSerial{Serial, main_, chambers};
Ui ui{main_, persistent, SCREEN_ADDR};
PhysicalUi physicalUi{
	main_,
	{.stopButton = stopInput, .rotationButton = rotationInput}};

void setup() {
	Serial.begin(115200);
	persistent.begin();

	persistent.restore();
	auto& config = persistent.inner.config;

	uiSerial.begin();
	ui.begin();
	tempController.begin();
	main_.setConfig(config);

	log_(version);
}

auto avgUiTick = 0.0, avgMainTick = 0.0, avgTotalTick = 0.0;
Timer statsTimer{1_s};

void loop() {
	auto now = Timestamp{millis()};

	uiSerial.tick(now);

	auto uiStart = Timestamp{millis()};
	ui.tick(now);
	auto uiEnd = Timestamp{millis()};

	physicalUi.tick(now);
	auto mainStart = Timestamp{millis()};
	main_.tick(now);
	auto mainEnd = Timestamp{millis()};

	auto uiDur = uiEnd - uiStart;
	auto mainDur = mainEnd - mainStart;
	auto totalDur = mainEnd - now;

	avgUiTick = avgUiTick * 0.9 + uiDur.unsafeGetValue() * 0.1;
	avgMainTick = avgMainTick * 0.9 + mainDur.unsafeGetValue() * 0.1;
	avgTotalTick = avgTotalTick * 0.9 + totalDur.unsafeGetValue() * 0.1;

	if (statsTimer.isDone(now) && STATS_ENABLED) {
		statsTimer.reset(now);
		printf(
			"Stats: ui = %.3f, uiCurrState = %.3f, uiInput = %.3f, "
			"uiSendLamps = %.3f, uiSendStrings = %.3f, uiReqPv = %.3f, "
			"main = %.3f, total = %.3f\n",
			avgUiTick, ui.avgCurrState, ui.avgInput, ui.avgSendLamps,
			ui.avgSendStrings, ui.avgReqPv, avgMainTick, avgTotalTick);
	}
}
