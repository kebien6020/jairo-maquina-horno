#pragma once

#include <Arduino.h>

namespace kev {

enum struct InputMode {
	NoPull = INPUT,
	PullUp = INPUT_PULLUP,
};

enum struct Invert {
	Normal,
	Inverted,
};

template <typename = void>
struct InputImpl {
	InputImpl(int pin,
			  Invert invert = Invert::Normal,
			  InputMode mode = InputMode::NoPull)
		: pin{pin}, invert{invert} {
		pinMode(pin, static_cast<uint8_t>(mode));
	}

	// Disable copy, as it's not clear if it should reset the pin mode
	InputImpl(InputImpl const&) = delete;
	auto operator=(InputImpl const&) -> InputImpl& = delete;

	// Move is fine though
	InputImpl(InputImpl&& o) : pin{o.pin}, invert{o.invert} {}
	auto operator=(InputImpl&& o) -> InputImpl& {
		pin = o.pin;
		invert = o.invert;
		return *this;
	}

	auto read() -> bool {
		return invert == Invert::Normal ? digitalRead(pin) == HIGH
										: digitalRead(pin) == LOW;
	}

	auto operator()() -> bool { return read(); }
	operator bool() { return read(); }

	auto set(bool value) -> void {
		digitalWrite(pin, invert == Invert::Normal ? value : !value);
	}

	auto operator=(bool value) -> InputImpl& {
		set(value);
		return *this;
	}

   private:
	int pin;
	Invert invert;
};

using Input = InputImpl<>;

template <typename = void>
struct OutputImpl {
	OutputImpl(int pin, Invert invert = Invert::Normal)
		: pin{pin}, invert{invert} {
		pinMode(pin, OUTPUT);
		write(false);
	}

	// Disable copy, as it's not clear if it should reset the pin mode
	OutputImpl(OutputImpl const&) = delete;
	auto operator=(OutputImpl const&) -> OutputImpl& = delete;

	// Move is fine though
	OutputImpl(OutputImpl&& o) : pin{o.pin}, invert{o.invert} {}
	auto operator=(OutputImpl&& o) -> OutputImpl& {
		pin = o.pin;
		invert = o.invert;
		return *this;
	}

	auto read() -> bool {
		return invert == Invert::Normal ? digitalRead(pin) == HIGH
										: digitalRead(pin) == LOW;
	}

	auto operator()() -> bool { return read(); }
	operator bool() { return read(); }

	auto write(bool value) -> void {
		digitalWrite(pin, invert == Invert::Normal ? value : !value);
	}

	auto operator=(bool value) -> OutputImpl& {
		write(value);
		return *this;
	}

   private:
	int pin;
	Invert invert;
};

using Output = OutputImpl<>;

template <int N>
struct RepeatedOutput {
	static_assert(N > 0, "Create with at least one output");

	// Disable copy, as it's not clear if it should reset the pin mode
	RepeatedOutput(RepeatedOutput const&) = delete;
	auto operator=(RepeatedOutput const&) -> RepeatedOutput& = delete;

	// Move is fine though
	RepeatedOutput(RepeatedOutput&& o) : outputs{o.outputs} {}
	auto operator=(RepeatedOutput&& o) -> RepeatedOutput& {
		outputs = o.outputs;
		return *this;
	}

	template <typename... Args>
	RepeatedOutput(Args&&... args) : outputs{std::forward<Args>(args)...} {}

	auto write(bool value) -> void {
		for (auto& output : outputs) {
			output.write(value);
		}
	}

	auto operator=(bool value) -> RepeatedOutput& {
		write(value);
		return *this;
	}

	auto read() -> bool { return outputs[0].read(); }
	operator bool() { return read(); }
	auto operator()() -> bool { return read(); }

   private:
	std::array<Output, N> outputs;
};

}  // namespace kev
