#pragma once

#include <SPI.h>
#include <optional>

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
	SPISettings{4000000, SPI_MSBFIRST, SPI_MODE0};

template <typename = void>
struct TempSensorMax6675 {
	TempSensorMax6675(SPIClass& spi, int csPin) : spi(spi) {
		spi.begin();
		spi.setHwCs(csPin);
	}

	auto getTemp(Timestamp now) -> std::optional<double> {
		if (forcedTemp) {
			return forcedTemp;
		}

		if (lastTemp && !pollInterval.isDone(now)) {
			return lastTemp;
		}

		auto const read = readTemp();
		if (read) {
			lastTemp = *read;
		}

		return lastTemp;
	}

	auto forceTemp(double temp) -> void { forcedTemp = temp; }
	auto unforceTemp() -> void { forcedTemp = {}; }

   private:
	auto readTemp() -> std::optional<double> {
		spi.beginTransaction(max6675Settings);
		auto raw = spi.transfer16(0);
		spi.endTransaction();

		if (raw & 0x4) {
			return {};  // No thermocouple connected
		}
		// Raw data comes in quarters of °C
		return (raw >> 3) * 0.25;
	}

	SPIClass& spi;
	Timer pollInterval{100_ms};
	std::optional<double> lastTemp = {};
	std::optional<double> forcedTemp = {};
};

using TempSensor = TempSensorMax6675<>;

}  // namespace kev
