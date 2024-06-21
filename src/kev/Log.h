#pragma once
#include <string_view>
#include <type_traits>
#include "HardwareSerial.h"

#define INLINE __attribute__((always_inline)) inline

namespace kev {

INLINE auto print() -> void {}

template <class T, class... Args>
INLINE auto print(T head, Args... tail) -> void {
	if constexpr (std::is_same_v<std::remove_reference_t<T>,
								 std::string_view>) {
		Serial.write(head.data(), head.size());
	} else {
		Serial.print(head);
	}
	print(tail...);
}

template <class... Args>
INLINE auto println(Args... args) -> void {
	print(args..., '\n');
}

template <bool enabled = true>
struct Log {
	Log(char const* name) : name{name} {}

	template <class... Args>
	INLINE auto operator()(Args... args) -> void {
		if (enabled) {
			println('[', name, ']', ' ', args...);
		}
	}

	template <class... Args>
	INLINE auto partial_start() -> void {
		if (enabled) {
			print('[', name, ']', ' ');
		}
	}

	template <class... Args>
	INLINE auto partial_end() -> void {
		if (enabled) {
			println();
		}
	}

	template <class... Args>
	INLINE auto partial(Args... args) -> void {
		if (enabled) {
			print(args...);
		}
	}

   private:
	char const* name;
};

#undef INLINE

}  // namespace kev
