#pragma once

#include <algorithm>
#include <functional>
#include <limits>
#include <numeric>
#include <optional>
#include "ConfigCommon.h"
#include "kev/Log.h"
#include "kev/Pin.h"
#include "kev/Time.h"
#include "kev/Timer.h"

#include "Chamber.h"
#include "Rotation.h"

using kev::Duration;
using kev::Log;
using kev::Output;
using kev::Timer;
using kev::Timestamp;
using std::accumulate;
using std::array;
using std::for_each;
using std::numeric_limits;
using std::optional;
using std::reference_wrapper;

using namespace kev::literals;

using Heater = kev::RepeatedOutput<2>;

enum class MainState {
	Idle,
	Preheating,
	Stage1,
	Stage2,
	Stage3,
};

enum class RotationState {
	Normal,
	ForceForward,
	ForceBackward,
};

struct PauseData {
	MainState state;
	Duration elapsed;
};

template <typename = void>
struct MainImpl {
	MainImpl(Heater& heater, array<Chamber, 3>& chambers, Rotation& rotation)
		: heater{heater}, chambers{chambers}, rotation{rotation} {}

	auto setConfig(Config cfg) -> void {
		preheatTemp = cfg.preheatTemp;
		chamberTempHist = cfg.chamberTempHist;
		stage1Temp = cfg.stages[0].temp;
		stage1Timer.setPeriod(cfg.stages[0].duration);
		stage2Temp = cfg.stages[1].temp;
		stage2Timer.setPeriod(cfg.stages[1].duration);
		stage3Temp = cfg.stages[2].temp;
		stage3Timer.setPeriod(cfg.stages[2].duration);
	}

	auto getConfig() -> Config {
		return Config{
			.preheatTemp = preheatTemp,
			.chamberTempHist = chamberTempHist,
			.stages =
				{
					StageConfig{.temp = stage1Temp,
								.duration = stage1Timer.getPeriod()},
					StageConfig{.temp = stage2Temp,
								.duration = stage2Timer.getPeriod()},
					StageConfig{.temp = stage3Temp,
								.duration = stage3Timer.getPeriod()},
				},
		};
	}

	auto setPreheatTemp(double temp) -> void { preheatTemp = temp; }

	auto tick(Timestamp now) -> void {
		if (state != prevState) {
			processStateChange(now);
		}

		processCurrentState(now);

		prevState = state;
	}

	auto eventUiPreheat(Timestamp now) -> void {
		switch (state) {
		case MainState::Idle: changeState(MainState::Preheating, now); break;
		case MainState::Preheating:
		case MainState::Stage1:
		case MainState::Stage2:
		case MainState::Stage3: break;
		}
	}
	auto eventUiStop(Timestamp now) -> void {
		changeState(MainState::Idle, now);
	}

	auto eventUiPause(Timestamp now) -> void {
		switch (state) {
		case MainState::Idle:
		case MainState::Preheating: break;
		case MainState::Stage1:
			pauseData = PauseData{MainState::Stage1, stage1Timer.elapsed(now)};
			break;
		case MainState::Stage2:
			pauseData = PauseData{MainState::Stage2, stage2Timer.elapsed(now)};
			break;
		case MainState::Stage3:
			pauseData = PauseData{MainState::Stage3, stage3Timer.elapsed(now)};
			break;
		}

		log("saved pause data: state = ", stateStr(pauseData->state),
			", elapsed = ", pauseData->elapsed.unsafeGetValue(), "ms");

		changeState(MainState::Idle, now);
	}

	auto eventUiStart(Timestamp now) -> void {
		switch (state) {
		case MainState::Idle:
			if (!pauseData) {
				log("start without pause data");
				changeState(MainState::Stage1, now);
			} else {
				log("starting with pause data");
				auto const state = pauseData->state;
				auto const elapsed = pauseData->elapsed;
				changeState(state, now);
				// Avoid resetting the timer on the next round
				prevState = state;

				switch (state) {
				case MainState::Stage1:
					stage1Timer.setElapsed(now, elapsed);
					break;
				case MainState::Stage2:
					stage2Timer.setElapsed(now, elapsed);
					break;
				case MainState::Stage3:
					stage3Timer.setElapsed(now, elapsed);
					break;
				}
				pauseData = {};
			}
			break;
		case MainState::Preheating:
		case MainState::Stage1:
		case MainState::Stage2:
		case MainState::Stage3: break;
		}
	}

	auto eventUiRotateFw() -> void {
		rotationState = RotationState::ForceForward;
	}
	auto eventUiRotateFwStop() -> void {
		rotationState = RotationState::Normal;
	}
	auto eventUiRotateBw() -> void {
		rotationState = RotationState::ForceBackward;
	}
	auto eventUiRotateBwStop() -> void {
		rotationState = RotationState::Normal;
	}

	auto stateStr(MainState state) -> char const* {
		switch (state) {
		case MainState::Idle: return "Idle";
		case MainState::Preheating: return "Preheating";
		case MainState::Stage1: return "Stage1";
		case MainState::Stage2: return "Stage2";
		case MainState::Stage3: return "Stage3";
		}
		return "Unknown";
	}

	auto displayState() -> char const* {
		switch (state) {
		case MainState::Idle: return "Detenido";
		case MainState::Preheating: return "Precalentando";
		case MainState::Stage1: return "Etapa Inicial";
		case MainState::Stage2: return "Etapa Intermedia";
		case MainState::Stage3: return "Etapa Final";
		}
		return "Desconocido";
	}

	auto readStateStr() -> char const* { return stateStr(state); }
	auto readHeater() -> bool { return heater.read(); }
	auto readFan(int i) -> bool { return chambers[i].fan.read(); }
	auto readTemp(int i, Timestamp now) -> optional<double> {
		return chambers[i].sensor.getTemp(now);
	}
	auto readRotation() -> bool { return rotation.enable.read(); }
	auto readRotationDir() -> bool { return rotation.direction.read(); }
	auto readCurrentTimer(Timestamp now) -> optional<Duration> {
		switch (state) {
		case MainState::Stage1: return stage1Timer.elapsed(now);
		case MainState::Stage2: return stage2Timer.elapsed(now);
		case MainState::Stage3: return stage3Timer.elapsed(now);
		default: return {};
		}
	}

   private:
	auto changeState(MainState newState, Timestamp now) -> void {
		state = newState;
		processStateChange(now);
	}

	auto processStateChange(Timestamp now) -> void {
		log("state change: ", stateStr(prevState), " -> ", stateStr(state));

		switch (state) {
		case MainState::Idle:
			heater.write(false);
			for_each(chambers.begin(), chambers.end(),
					 [](Chamber& ch) { ch.fan.write(false); });
			rotation.stop();
			break;
		case MainState::Preheating: heater.write(true); break;
		case MainState::Stage1:
			heater.write(true);
			rotation.start_fw();
			stage1Timer.reset(now);
			break;
		case MainState::Stage2:
			heater.write(true);
			rotation.start_fw();
			stage2Timer.reset(now);
			break;
		case MainState::Stage3:
			heater.write(true);
			rotation.start_fw();
			stage3Timer.reset(now);
			break;
		}
	}

	auto processCurrentState(Timestamp now) -> void {
		switch (state) {
		case MainState::Idle: break;
		case MainState::Preheating:
			if (minTemp(now) >= preheatTemp) {
				changeState(MainState::Idle, now);
				log("preheat finished due to temperature");
				return;
			}

			for (auto& ch : chambers) {
				controlChamberWithHisteresis(ch, preheatTemp, now);
			}
			break;

		case MainState::Stage1:
			if (stage1Timer.isDone(now)) {
				stage1Timer.reset(now);
				changeState(MainState::Stage2, now);
			}
			for (auto& ch : chambers) {
				controlChamberWithHisteresis(ch, stage1Temp, now);
			}
			controlHeaterWithHisteresis(stage1Temp, now);
			break;
		case MainState::Stage2:
			if (stage2Timer.isDone(now)) {
				stage2Timer.reset(now);
				changeState(MainState::Stage3, now);
			}
			for (auto& ch : chambers) {
				controlChamberWithHisteresis(ch, stage2Temp, now);
			}
			controlHeaterWithHisteresis(stage2Temp, now);
			break;

		case MainState::Stage3:
			if (stage3Timer.isDone(now)) {
				stage3Timer.reset(now);
				changeState(MainState::Idle, now);
			}
			for (auto& ch : chambers) {
				controlChamberWithHisteresis(ch, stage3Temp, now);
			}
			controlHeaterWithHisteresis(stage3Temp, now);
			break;
		}

		switch (state) {
		case MainState::Idle:
		case MainState::Preheating:
			switch (rotationState) {
			case RotationState::Normal: rotation.stop(); break;
			case RotationState::ForceForward: rotation.start_fw(); break;
			case RotationState::ForceBackward: rotation.start_bw(); break;
			}
		}
	}

	auto minTemp(Timestamp now) -> optional<double> {
		auto const t1 = chambers[0].sensor.getTemp(now);
		if (!t1) {
			return {};
		}
		auto const t2 = chambers[1].sensor.getTemp(now);
		if (!t2) {
			return {};
		}
		auto const t3 = chambers[2].sensor.getTemp(now);
		if (!t3) {
			return {};
		}

		return std::min({*t1, *t2, *t3});
	}

	auto controlChamberWithHisteresis(Chamber& ch,
									  double targetTemp,
									  Timestamp now) -> void {
		auto const temp = ch.sensor.getTemp(now);
		if (!temp) {
			log("failed to read temp");
			ch.fan.write(false);
			return;
		}

		auto const isOn = ch.fan.read();
		auto const low = targetTemp - chamberTempHist;
		auto const high = targetTemp;

		if (isOn && temp > high) {
			ch.fan.write(false);
		} else if (!isOn && temp < low) {
			ch.fan.write(true);
		}
	}

	auto controlHeaterWithHisteresis(double targetTemp, Timestamp now) -> void {
		auto const temp = minTemp(now);
		if (!temp) {
			log("failed to read temp");
			heater.write(false);
			return;
		}

		auto const isOn = heater.read();
		auto const low = targetTemp - chamberTempHist;
		auto const high = targetTemp;

		if (isOn && temp > high) {
			heater.write(false);
		} else if (!isOn && temp < low) {
			heater.write(true);
		}
	}

	Log<> log{"main"};
	RotationState rotationState = RotationState::Normal;
	MainState state = MainState::Idle;
	MainState prevState = MainState::Idle;
	std::optional<PauseData> pauseData = {};
	Timer stage1Timer = {{}};
	Timer stage2Timer = {{}};
	Timer stage3Timer = {{}};

	double preheatTemp;
	double chamberTempHist;
	double stage1Temp;
	double stage2Temp;
	double stage3Temp;

	Heater& heater;
	std::array<Chamber, 3>& chambers;
	Rotation& rotation;
};

using Main = MainImpl<>;
