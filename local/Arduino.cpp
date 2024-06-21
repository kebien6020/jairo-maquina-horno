#include "Arduino.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <string>
#include <thread>

#include "Preferences.h"

using std::ifstream;
using std::ofstream;
using std::printf;
using std::string;
using std::terminate;
using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;
using clk = std::chrono::steady_clock;

void HardwareSerial::begin(int) {
	initialized = true;
}

void delayMicroseconds(unsigned long us) {
	sleep_for(microseconds{us});
}

auto millis() -> unsigned long {
	return duration_cast<milliseconds>(clk::now().time_since_epoch()).count();
}

auto pinModes = std::array<uint8_t, 256>{};
auto pinState = std::array<uint8_t, 256>{};

void pinMode(uint8_t pin, uint8_t mode) {
	pinModes[pin] = mode;
}

void digitalWrite(uint8_t pin, uint8_t val) {
	if (pinModes[pin] != OUTPUT) {
		// Ignore writes to input pins, just like real hardware does
		return;
	}

	pinState[pin] = val;
}

auto digitalRead(uint8_t pin) -> uint8_t {
	return pinState[pin];
}

void setup();
void loop();

auto main() -> int {
	setup();
	while (true) {
		loop();
	}
}

HardwareSerial Serial;
HardwareSerial Serial2;

// Preferences.h

Preferences::Preferences() = default;
Preferences::~Preferences() = default;

auto Preferences::begin(char const* name, bool readOnly, char const*) -> bool {
	this->name = name;
	this->readOnly = readOnly;
	return true;
}

auto Preferences::end() -> void {}

auto Preferences::remove(char const* key) -> bool {
	if (readOnly) {
		printf("Preferences::remove called on read-only preferences\n");
		terminate();
	}

	auto fname = string{name} + "_" + string{key} + ".bin";
	std::remove(fname.c_str());
	return true;
}

auto Preferences::putBytes(char const* key, void const* value, size_t len)
	-> size_t {
	if (readOnly) {
		printf("Preferences::putBytes called on read-only preferences\n");
		terminate();
	}

	auto fname = string{name} + "_" + string{key} + ".bin";
	auto file = ofstream{fname, ofstream::binary};
	file.write(static_cast<char const*>(value), static_cast<long>(len));
	return len;
}

auto Preferences::getBytes(char const* key, void* buf, size_t maxLen)
	-> size_t {
	auto fname = string{name} + "_" + string{key} + ".bin";
	auto file = ifstream{fname, ifstream::binary};
	if (!file) {
		return 0;
	}

	file.read(static_cast<char*>(buf), static_cast<long>(maxLen));
	return maxLen;
}
