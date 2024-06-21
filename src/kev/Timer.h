#pragma once
#include "Time.h"

namespace kev {

struct Timer {
	Timer(Duration setting) : setting{setting} {}

	auto reset(Timestamp now) -> void { last = now; }

	auto isDone(Timestamp now) -> bool { return (now - last) > setting; }

	auto elapsedSec(Timestamp now) -> long {
		return (now - last).unsafeGetValue() / 1000;
	}
	auto elapsed(Timestamp now) -> Duration { return now - last; }
	auto totalSec() -> long { return setting.unsafeGetValue() / 1000; }

	auto setElapsed(Timestamp now, Duration elapsed) -> void {
		last = now - elapsed;
	}

	auto getPeriod() -> Duration { return setting; }
	auto setPeriod(Duration period) -> void { setting = period; }

   private:
	Timestamp last = {};
	Duration setting;
};

}  // namespace kev
