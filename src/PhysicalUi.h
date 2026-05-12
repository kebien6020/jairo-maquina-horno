#pragma once

#include "Main.h"
#include "kev/Edge.h"
#include "kev/Pin.h"
#include "kev/Time.h"

using kev::Edge;
using kev::Input;
using kev::EdgeDebounced;
using namespace kev::literals;

struct PhysicalUiPinout {
	Input& stopButton;
	Input& rotationButton;
};

template <typename = void>
struct PhysicalUiImpl {
	PhysicalUiImpl(Main& main, PhysicalUiPinout pinout)
		: main{main},
		  pinout{std::move(pinout)},
		  stopButtonEdge{pinout.stopButton, 1_s},
		  rotationButtonEdge{pinout.rotationButton} {}

	auto tick(Timestamp now) -> void {
		stopButtonEdge.update(now);
		rotationButtonEdge.update();

		if (stopButtonEdge.risingEdge()) {
			main.eventUiStart(now);
			log("starting because of button press");
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
	EdgeDebounced<Input> stopButtonEdge;
	Edge<Input> rotationButtonEdge;

	Log<> log{"physical_ui"};
};

using PhysicalUi = PhysicalUiImpl<>;
