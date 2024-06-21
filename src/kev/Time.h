#pragma once

namespace kev {

class Duration {
	long value = 0;

   public:
	constexpr Duration() = default;
	constexpr Duration(long value) : value{value} {}
	[[nodiscard]] constexpr auto unsafeGetValue() const -> long {
		return value;
	}

	friend constexpr auto operator<(Duration const&, Duration const&) -> bool;
	friend constexpr auto operator==(Duration const&, Duration const&) -> bool;
};

inline constexpr auto operator==(Duration const& lhs,
								 Duration const& rhs) -> bool {
	return lhs.value == rhs.value;
}
inline constexpr auto operator!=(Duration const& lhs,
								 Duration const& rhs) -> bool {
	return !operator==(lhs, rhs);
}
inline constexpr auto operator<(Duration const& lhs,
								Duration const& rhs) -> bool {
	return lhs.value < rhs.value;
}
inline constexpr auto operator>(Duration const& lhs,
								Duration const& rhs) -> bool {
	return operator<(rhs, lhs);
}
inline constexpr auto operator<=(Duration const& lhs,
								 Duration const& rhs) -> bool {
	return !operator>(lhs, rhs);
}
inline constexpr auto operator>=(Duration const& lhs,
								 Duration const& rhs) -> bool {
	return !operator<(lhs, rhs);
}
inline constexpr auto operator+(Duration const& lhs,
								Duration const& rhs) -> Duration {
	return Duration{lhs.unsafeGetValue() + rhs.unsafeGetValue()};
}

namespace literals {

inline constexpr auto operator""_ms(unsigned long long t) -> Duration {
	return Duration{static_cast<long>(t)};
}
inline constexpr auto operator""_s(unsigned long long t) -> Duration {
	return Duration{static_cast<long>(t * 1000ul)};
}
inline constexpr auto operator""_min(unsigned long long t) -> Duration {
	return Duration{static_cast<long>(t * 60ul * 1000ul)};
}
inline constexpr auto operator""_h(unsigned long long t) -> Duration {
	return Duration{static_cast<long>(t * 60ul * 60ul * 1000ul)};
}

}  // namespace literals

class Timestamp {
	unsigned long value = 0;

   public:
	constexpr Timestamp() = default;
	constexpr Timestamp(unsigned long value) : value{value} {}
	friend constexpr auto operator-(Timestamp const&,
									Timestamp const&) -> Duration;
	friend constexpr auto operator+(Timestamp const&,
									Duration const&) -> Timestamp;
	friend constexpr auto operator-(Timestamp const&,
									Duration const&) -> Timestamp;

	constexpr explicit operator bool() { return value != 0ul; }
};

constexpr auto operator-(Timestamp const& lhs,
						 Timestamp const& rhs) -> Duration {
	return {static_cast<long>(lhs.value - rhs.value)};
}

constexpr auto operator+(Timestamp const& stamp,
						 Duration const& dur) -> Timestamp {
	return {stamp.value + static_cast<unsigned long>(dur.unsafeGetValue())};
}

constexpr auto operator-(Timestamp const& stamp,
						 Duration const& dur) -> Timestamp {
	return {stamp.value - static_cast<unsigned long>(dur.unsafeGetValue())};
}

}  // namespace kev
