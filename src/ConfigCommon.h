#pragma once

#include <array>
#include "kev/Time.h"

struct StageConfig {
	double temp;
	kev::Duration duration;
};

struct Config {
	double preheatTemp;
	double chamberTempHist;
	std::array<StageConfig, 3> stages;
};

enum class MainState {
	Idle,
	Preheating,
	Stage1,
	Stage2,
	Stage3,
};

struct PauseData {
	MainState state;
	kev::Duration elapsed;
};
