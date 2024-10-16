#pragma once

#include <algorithm>
#include <functional>
#include <limits>
#include <numeric>
#include <optional>

#include "Chamber.h"
#include "ConfigCommon.h"
#include "Rotation.h"
#include "State.h"
#include "kev/AutonicsTempController.h"
#include "kev/Log.h"
#include "kev/Pin.h"
#include "kev/Time.h"
#include "kev/Timer.h"

using kev::AutonicsTempController;
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

constexpr auto HEATER_FAILURE_TIMEOUT = 2_min;
constexpr auto HEATER_FAILURE_TEMP_DIFF = 2.0;

enum class RotationState {
	Normal,
	ForceForward,
	ForceBackward,
};

template <typename = void>
struct MainImpl {
	MainImpl(array<Chamber, 3>& chambers,
			 Rotation& rotation,
			 AutonicsTempController& tempController,
			 State& persistent)
		: chambers{chambers},
		  rotation{rotation},
		  tempController{tempController},
		  persistent{persistent} {}

	auto setConfig(Config cfg) -> void {
		preheatTemp = cfg.preheatTemp;
		chamberTempHist = cfg.chamberTempHist;
		stage1Temp = cfg.stages[0].temp;
		stage1Timer.setPeriod(cfg.stages[0].duration);
		stage2Temp = cfg.stages[1].temp;
		stage2Timer.setPeriod(cfg.stages[1].duration);
		stage3Temp = cfg.stages[2].temp;
		stage3Timer.setPeriod(cfg.stages[2].duration);

		applyHeaterTemperature();
	}

	auto setPauseData(std::optional<PauseData> pauseData, Timestamp now) -> void {
		this->pauseData = pauseData;

		// Restore the state
		restorePauseData(now);
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
		savePauseData(now);

		changeState(MainState::Idle, now);
	}

	auto eventUiStart(Timestamp now) -> void {
		switch (state) {
		case MainState::Idle:
			if (!pauseData) {
				log("start without pause data");
				changeState(MainState::Stage1, now);
			} else {
				restorePauseData(now);
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
	auto readHeater(Timestamp) -> bool { return tempController.isRunning(); }
	auto readFan(int i) -> bool { return chambers[i].fan.read(); }
	auto readTemp(int i, Timestamp now) -> optional<double> {
		return chambers[i].sensor.getTemp(now);
	}
	auto readRotation() -> bool {
		return rotation.fw.read() || rotation.bw.read();
	}
	auto readRotationDir() -> bool { return rotation.bw.read(); }
	auto readCurrentTimer(Timestamp now) -> optional<Duration> {
		switch (state) {
		case MainState::Stage1: return stage1Timer.elapsed(now);
		case MainState::Stage2: return stage2Timer.elapsed(now);
		case MainState::Stage3: return stage3Timer.elapsed(now);
		default: return {};
		}
	}

	auto heaterTemp(Timestamp now) -> optional<double> {
		auto const temp = tempController.readPv(now);
		if (!temp) {
			log("failed to read temp, falling back to sensor temp");
			return minTemp(now);
		}
		chambers[0].sensor.forceTemp(*temp + 10);
		chambers[1].sensor.forceTemp(*temp +  5);
		chambers[2].sensor.forceTemp(*temp - 10);
		return *temp;
	}

   private:
	auto changeState(MainState newState, Timestamp now) -> void {
		state = newState;
		processStateChange(now);
	}

	auto processStateChange(Timestamp now) -> void {
		log("state change: ", stateStr(prevState), " -> ", stateStr(state));

		applyHeaterTemperature();

		switch (state) {
		case MainState::Idle:
			tempController.setRun(false);
			for_each(chambers.begin(), chambers.end(),
					 [](Chamber& ch) { ch.fan.write(false); });
			rotation.stop();
			break;
		case MainState::Preheating: tempController.setRun(true); break;
		case MainState::Stage1:
			tempController.setRun(true);
			rotation.start_fw();
			stage1Timer.reset(now);
			break;
		case MainState::Stage2:
			tempController.setRun(true);
			rotation.start_fw();
			stage2Timer.reset(now);
			break;
		case MainState::Stage3:
			tempController.setRun(true);
			rotation.start_fw();
			stage3Timer.reset(now);
			break;
		}
	}

	auto savePauseData(Timestamp now) -> void {
		switch (state) {
		case MainState::Idle: break;
		case MainState::Preheating:
			pauseData = PauseData{MainState::Preheating, {}};
			break;
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

		persistent.inner.pauseData = pauseData;
		persistent.persist();

		log("saved pause data: state = ", stateStr(pauseData->state),
			", elapsed = ", pauseData->elapsed.unsafeGetValue(), "ms");
	}

	auto restorePauseData(Timestamp now) -> void {
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

	auto applyHeaterTemperature() -> void {
		switch (state) {
		case MainState::Idle: break;
		case MainState::Preheating: tempController.setSv(preheatTemp); break;
		case MainState::Stage1: tempController.setSv(stage1Temp); break;
		case MainState::Stage2: tempController.setSv(stage2Temp); break;
		case MainState::Stage3: tempController.setSv(stage3Temp); break;
		}
	}

	auto processCurrentState(Timestamp now) -> void {
		// Main control
		switch (state) {
		case MainState::Idle:
			for_each(chambers.begin(), chambers.end(),
					 [](Chamber& ch) { ch.fan.write(false); });
			rotation.stop();
			break;
		case MainState::Preheating:
			if (heaterTemp(now) >= preheatTemp) {
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
			break;
		case MainState::Stage2:
			if (stage2Timer.isDone(now)) {
				stage2Timer.reset(now);
				changeState(MainState::Stage3, now);
			}
			for (auto& ch : chambers) {
				controlChamberWithHisteresis(ch, stage2Temp, now);
			}
			break;

		case MainState::Stage3:
			if (stage3Timer.isDone(now)) {
				stage3Timer.reset(now);
				changeState(MainState::Idle, now);
			}
			for (auto& ch : chambers) {
				controlChamberWithHisteresis(ch, stage3Temp, now);
			}
			break;
		}

		// Rotation
		switch (state) {
		case MainState::Idle:
		case MainState::Preheating:
			switch (rotationState) {
			case RotationState::Normal: rotation.stop(); break;
			case RotationState::ForceForward: rotation.start_fw(); break;
			case RotationState::ForceBackward: rotation.start_bw(); break;
			}
		}

		// Track the info required to detect heater failure
		switch (state) {
			case MainState::Preheating:
			case MainState::Stage1:
			case MainState::Stage2:
			case MainState::Stage3:
				detectHeaterFailure(now);
				break;
		}

		// Persist pause data
		if (pausePersistTimer.isDone(now)) {
			pausePersistTimer.reset(now);

			savePauseData(now);
		}

	}

	auto detectHeaterFailure(Timestamp now) -> void {
		// Track last transition of the heater output
		auto const isOn = tempController.readOut1(now);
		if (isOn.has_value()) {
			if (*isOn && !isHeating) {
				lastOutputTransition = now;
				auto const temp = tempController.readPv(now);
				if (temp) {
					lastTransitionTemp = *temp;
				}
			}

			isHeating = *isOn;
		}

		// Actually detect the failure
		auto const elapsed = now - lastOutputTransition;
		auto const timePassed = elapsed > HEATER_FAILURE_TIMEOUT;
		auto const currentTemp = tempController.readPv(now);
		if (!currentTemp) {
			log("failed to read temp");
			return;
		}

		auto const tempDiff = *currentTemp - lastTransitionTemp;
		auto const badTempDiff = tempDiff < HEATER_FAILURE_TEMP_DIFF;

		if (isHeating && timePassed && badTempDiff) {
			log("heater failure detected trying to get the controller to retry");
			tempController.setRun(false);
			delay(1000);
			tempController.setRun(true);

			lastOutputTransition = now;
			lastTransitionTemp = *currentTemp;
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

	Log<> log{"main"};
	RotationState rotationState = RotationState::Normal;
	MainState state = MainState::Idle;
	MainState prevState = MainState::Idle;
	std::optional<PauseData> pauseData = {};
	Timer stage1Timer = {{}};
	Timer stage2Timer = {{}};
	Timer stage3Timer = {{}};
	Timer pausePersistTimer = {5_min};

	double preheatTemp;
	double chamberTempHist;
	double stage1Temp;
	double stage2Temp;
	double stage3Temp;

	Timestamp lastOutputTransition = 0;
	double lastTransitionTemp = 0;
	bool isHeating = false;

	std::array<Chamber, 3>& chambers;
	Rotation& rotation;
	AutonicsTempController& tempController;
	State& persistent;
};

using Main = MainImpl<>;
