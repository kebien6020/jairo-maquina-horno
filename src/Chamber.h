#pragma once

#include "kev/Pin.h"
#include "kev/TempSensor.h"

using kev::Output;
using kev::TempSensor;

struct Chamber {
	TempSensor sensor;
	Output fan;
};
