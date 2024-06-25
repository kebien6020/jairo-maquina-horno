#pragma once

#include "kev/Log.h"
#include "kev/Pin.h"

using kev::Output;

struct Rotation {
	auto stop() -> void {
		log("stop");
		fw.write(false);
		bw.write(false);
	}

	auto start_fw() -> void {
		log("start_fw");
		fw.write(true);
		bw.write(false);
	}

	auto start_bw() -> void {
		log("start_bw");
		fw.write(false);
		bw.write(true);
	}

	Output fw;
	Output bw;

	kev::Log<false> log{"rotation"};
};
