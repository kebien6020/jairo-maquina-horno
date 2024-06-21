#pragma once
#include <HardwareSerial.h>
#include <cstdint>

constexpr auto INPUT = 0;
constexpr auto INPUT_PULLUP = 1;
constexpr auto OUTPUT = 2;

constexpr auto LOW = 0;
constexpr auto HIGH = 1;

void delayMicroseconds(unsigned long us);
auto millis() -> unsigned long;

void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t value);
auto digitalRead(uint8_t pin) -> uint8_t;
