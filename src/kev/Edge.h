#pragma once

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

}  // namespace kev
