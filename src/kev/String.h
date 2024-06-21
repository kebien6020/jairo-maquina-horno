#pragma once

#include <Arduino.h>
#include <charconv>
#include <string_view>

#define INLINE __attribute__((always_inline)) inline

namespace kev {

INLINE auto tokens(std::string_view s,
				   char delim) -> std::vector<std::string_view> {
	auto tokens = std::vector<std::string_view>{};

	auto start = 0u;
	auto end = s.find_first_of(delim);

	while (end != std::string_view::npos) {
		tokens.push_back(s.substr(start, end - start));
		start = end + 1;
		end = s.find_first_of(delim, start);
	}

	tokens.push_back(s.substr(start));

	return tokens;
}

constexpr INLINE auto trim(std::string_view s) -> std::string_view {
	constexpr auto whitespace = " \t\r\n";
	auto start = s.find_first_not_of(whitespace);
	auto end = s.find_last_not_of(whitespace);

	if (start == std::string_view::npos) {
		return {};
	}

	return s.substr(start, end - start + 1);
}
static_assert(trim(" \t  hello  \r\n") == "hello");
static_assert(trim(" \t  hello world  \n") == "hello world");
static_assert(trim("ping\r\n") == "ping");
static_assert(trim("ping\r") == "ping");
static_assert(trim("") == "");

INLINE auto remove_backspaces(std::string_view s) -> std::string {
	auto result = std::string{};

	for (auto c : s) {
		if (c == '\b') {
			if (!result.empty()) {
				result.pop_back();
			}
		} else {
			result.push_back(c);
		}
	}

	return result;
}

INLINE auto parse_int(std::string_view s) -> std::optional<int> {
	auto out = 0;
	auto const res = std::from_chars(s.begin(), s.end(), out);
	if (res.ec == std::errc::invalid_argument) {
		return {};
	}

	return {out};
}

}  // namespace kev
