#pragma once

#include "Main.h"
#include "kev/Edge.h"
#include "kev/Pin.h"

using kev::Edge;
using kev::Input;

struct PhysicalUiPinout {
	Input& stopButton;
	Input& rotationButton;
};

template <typename = void>
struct PhysicalUiImpl {
	PhysicalUiImpl(Main& main, PhysicalUiPinout pinout)
		: main{main},
		  pinout{std::move(pinout)},
		  stopButtonEdge{pinout.stopButton},
		  rotationButtonEdge{pinout.rotationButton} {}

	auto tick(Timestamp now) -> void {
		stopButtonEdge.update();
		rotationButtonEdge.update();

		if (stopButtonEdge.risingEdge()) {
			// main.eventUiStop(now);
			log("stop button pressed");
		}

		if (rotationButtonEdge.risingEdge()) {
			// main.eventUiRotateFw();
		}

		if (rotationButtonEdge.fallingEdge()) {
			// main.eventUiRotateFwStop();
		}
	}

   private:
	Main& main;
	PhysicalUiPinout pinout;
	Edge<Input> stopButtonEdge;
	Edge<Input> rotationButtonEdge;

	Log<> log{"physical_ui"};
};

using PhysicalUi = PhysicalUiImpl<>;
