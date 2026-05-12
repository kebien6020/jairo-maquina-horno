#pragma once

#include "Log.h"
#include "Time.h"

namespace kev {

template <class Reader>
class Edge {
   public:
	Edge(Reader& reader, bool prev = false, bool curr = false)
		: reader{reader}, prev{prev}, curr{curr} {}

	auto update() -> void {
		prev = curr;
		curr = reader.read();
	}

	auto risingEdge() -> bool {
		auto const retval = changed() && curr;
		return retval;
	}

	auto fallingEdge() -> bool {
		auto const retval = changed() && !curr;
		return retval;
	}

	auto changed() -> bool { return prev != curr; }

	auto value() -> bool { return curr; }

   private:
	Reader& reader;
	bool prev;
	bool curr;
};

template <class Reader>
class EdgeDebounced {
   public:
	EdgeDebounced(Reader& reader,
				  Duration dur,
				  bool prev = false,
				  bool curr = false)
		: reader{reader}, dur{dur}, prev{prev}, curr{curr}, lastRaw{prev} {}

	auto update(Timestamp now) -> void {
		bool raw = reader.read();
		// Reset changed on every update
		_changed = false;

		if (raw != lastRaw) {
			lastChange = now;
			lastRaw = raw;
			log("Raw changed to ", raw ? "HIGH" : "LOW",
				", starting debounce timer");
		}

		if ((now - lastChange) >= dur && raw != curr) {
			prev = curr;
			curr = raw;
			// Keep it as true for a single tick
			_changed = true;
			log("Debounced to ", curr ? "HIGH" : "LOW");
		}
	}

	auto risingEdge() -> bool {
		auto const retval = changed() && curr;
		return retval;
	}

	auto fallingEdge() -> bool {
		auto const retval = changed() && !curr;
		return retval;
	}

	auto changed() -> bool { return _changed; }

	auto value() -> bool { return curr; }

   private:
	Reader& reader;
	Duration dur;
	Timestamp lastChange = 0;
	Log<false> log{"EdgeDebounced"};
	bool prev;
	bool curr;
	bool _changed = false;
	bool lastRaw;
};

}  // namespace kev
