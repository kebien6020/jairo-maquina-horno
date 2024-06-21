#pragma once

#include "kev/Pin.h"

using kev::Output;

struct Rotation {
	auto stop() -> void { enable.write(false); }

	auto start_fw() -> void {
		direction.write(false);
		enable.write(true);
	}

	auto start_bw() -> void {
		direction.write(true);
		enable.write(true);
	}

	Output direction;
	Output enable;
};
