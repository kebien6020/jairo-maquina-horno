#pragma once

#include <Preferences.h>

#include "kev/Log.h"
#include "kev/Time.h"

#include "ConfigCommon.h"

using kev::Duration;
using kev::Log;

using namespace kev::literals;

struct StatePOD {
	Config config;
};

template <typename = void>
struct StateImpl {
	StatePOD inner;

	StateImpl() { prefs.begin("main", false); }

	auto persist() -> void {
		prefs.putBytes("state", &inner, sizeof(inner));
		log("saved preferences");
	}

	auto restore() -> void {
		auto len = prefs.getBytes("state", &inner, sizeof(inner));
		if (len != sizeof(inner)) {
			inner = {};
		}

		log.partial_start();
		log.partial("restored preferences: State{");
		log.partial("\n  preheatTemp = ", inner.config.preheatTemp);
		log.partial("\n  chamberTempHist = ", inner.config.chamberTempHist);
		log.partial("\n}");
		log.partial_end();
	}

   private:
	Preferences prefs;
	Log<false> log{"state"};
};

using State = StateImpl<>;
