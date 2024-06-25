#pragma once

#include <SPI.h>
#include <cmath>
#include <optional>

#include "kev/Log.h"
#include "kev/Pin.h"
#include "kev/Timer.h"

namespace kev {

using namespace kev::literals;

template <typename = void>
struct TempSensorFakeImpl {
	auto getTemp(Timestamp) -> std::optional<double> { return temp; }
	auto forceTemp(double temp) -> void { this->temp = temp; }
	auto unforceTemp() -> void { this->temp = 20.0; }

	TempSensorFakeImpl() = default;

	// Disable copy as it probably doesn't make sense with the real one
	TempSensorFakeImpl(TempSensorFakeImpl const&) = delete;
	auto operator=(TempSensorFakeImpl const&) -> TempSensorFakeImpl& = delete;

	TempSensorFakeImpl(TempSensorFakeImpl&&) = default;
	auto operator=(TempSensorFakeImpl&&) -> TempSensorFakeImpl& = default;

   private:
	double temp = 20.0;
};

using TempSensorFake = TempSensorFakeImpl<>;

static auto const max6675Settings =
	SPISettings{1000000, SPI_MSBFIRST, SPI_MODE0};

template <typename = void>
struct TempSensorMax6675 {
	TempSensorMax6675(SPIClass& spi, int csPin)
		: spi{spi}, cs{csPin, Invert::Inverted} {
		spi.begin();
	}

	auto getTemp(Timestamp now) -> std::optional<double> {
		if (forcedTemp) {
			return forcedTemp;
		}

		if (lastTemp && !pollInterval.isDone(now)) {
			return lastTemp;
		}

		pollInterval.reset(now);

		auto const read = readTemp();
		if (read && lastTemp) {
			lastTemp = *lastTemp * 0.70 + *read * 0.30;
		} else if (read) {
			lastTemp = *read;
		}

		return lastTemp;
	}

	auto forceTemp(double temp) -> void { forcedTemp = temp; }
	auto unforceTemp() -> void { forcedTemp = {}; }

   private:
	auto readTemp() -> std::optional<double> {
		cs.write(true);
		delayMicroseconds(100);
		spi.beginTransaction(max6675Settings);
		auto raw = spi.transfer16(0);
		spi.endTransaction();
		cs.write(false);
		delayMicroseconds(100);

		if (raw & 0x4) {
			log("error reading cs = ", cs.getPin(), " raw = ", raw);
			return {};  // No thermocouple connected
		}

		auto const temp = (raw >> 3) * 0.25;

		if (lastTemp && std::abs(temp - *lastTemp) > 100.0) {
			log("temp sensor reading too different, ignoring. pin = ",
				cs.getPin(), " temp = ", temp);
			return {};
		}

		if (temp == 0.0) {
			log("temp sensor reading zero, ignoring. pin = ", cs.getPin());
			return {};
		}

		// Raw data comes in quarters of °C
		// Show data in binary
		log.partial_start();
		log.partial("raw = ");
		for (int i = 15; i >= 0; i--) {
			log.partial((raw >> i) & 1);
		}
		log.partial(", value = ", temp);
		log.partial(", pin = ", cs.getPin());
		log.partial_end();
		return temp;
	}

	SPIClass& spi;
	Timer pollInterval{500_ms};
	std::optional<double> lastTemp = {};
	std::optional<double> forcedTemp = {};
	Output cs;
	Log<false> log{"temp sensor"};
};

using TempSensor = TempSensorMax6675<>;

}  // namespace kev
