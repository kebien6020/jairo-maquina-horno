#pragma once

#include <cstdio>
#include <exception>
#include <iostream>

class HardwareSerial {
   public:
	void begin(int baudrate);

	template <typename T>
	void print(T t) {
		if (!initialized) {
			std::printf("Serial print called before begin\n");
			std::terminate();
			return;
		}

		std::cout << t;
	}

	template <typename... Args>
	void printf(char const* str, Args... args) {
		if (!initialized) {
			std::printf("Serial print called before begin\n");
			std::terminate();
			return;
		}

		std::printf(str, args...);
	}

   private:
	bool initialized = false;
};

extern HardwareSerial Serial;
extern HardwareSerial Serial2;
