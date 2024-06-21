#include <Arduino.h>
#include <SPI.h>

#include <array>

#include "PhysicalUi.h"
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

constexpr auto version = "Version 1.0 (2024-06-21)";

using namespace kev::literals;
using kev::Input;
using kev::InputMode;
using kev::Invert;
using kev::Log;
using kev::Output;
using kev::RepeatedOutput;
using kev::TempSensorFake;
using kev::Timer;
using kev::Timestamp;
using std::array;

constexpr auto FAN_PIN1 = 26;
constexpr auto FAN_PIN2 = 27;
constexpr auto FAN_PIN3 = 14;

constexpr auto SENSOR_CS_1 = 32;
constexpr auto SENSOR_CS_2 = 33;
constexpr auto SENSOR_CS_3 = 25;

constexpr auto PHY_STOP_PIN = 36;
constexpr auto PHY_ROTATION_PIN = 39;

auto heater = RepeatedOutput<2>{Output{12, Invert::Inverted},
								Output{13, Invert::Inverted}};
auto spi = SPIClass{VSPI};
auto chambers = array{
	Chamber{TempSensor{spi, SENSOR_CS_1}, Output{FAN_PIN1, Invert::Inverted}},
	Chamber{TempSensor{spi, SENSOR_CS_2}, Output{FAN_PIN2, Invert::Inverted}},
	Chamber{TempSensor{spi, SENSOR_CS_3}, Output{FAN_PIN3, Invert::Inverted}}};
auto rotation = Rotation{.direction = Output{4, Invert::Inverted},
						 .enable = Output{2, Invert::Inverted}};

auto main_ = Main{heater, chambers, rotation};

auto persistent = State{};

Log<> log_{"main"};
Timer logTimer{1_s};
Timer prefsTimer{10_s};
UiSerial uiSerial{Serial, main_, chambers};
Ui ui{main_, persistent};
PhysicalUi physicalUi{
	main_,
	{.stopButton = Input{PHY_STOP_PIN, Invert::Inverted, InputMode::PullUp},
	 .rotationButton =
		 Input{PHY_ROTATION_PIN, Invert::Inverted, InputMode::PullUp}}};

void setup() {
	Serial.begin(115200);
	persistent.restore();
	auto& config = persistent.inner.config;

	uiSerial.begin();
	ui.begin();
	main_.setConfig(config);

	log_(version);
}

void loop() {
	auto now = Timestamp{millis()};

	uiSerial.tick(now);
	ui.tick(now);
	main_.tick(now);

	if (prefsTimer.isDone(now)) {
		prefsTimer.reset(now);
		persistent.persist();
	}
}
