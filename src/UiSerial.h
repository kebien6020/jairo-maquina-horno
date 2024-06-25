#pragma once

#include <string_view>

#include "HardwareSerial.h"
#include "Main.h"
#include "kev/Log.h"
#include "kev/String.h"
#include "kev/Time.h"
#include "kev/Timer.h"

using kev::Timer;
using kev::Timestamp;
using std::array;
using std::string_view;
using std::vector;

using namespace kev::literals;
using chambers_t = array<Chamber, 3>;

struct UiSerial {
	UiSerial(HardwareSerial& serial, Main& main, chambers_t& chambers)
		: serial{serial}, main{main}, chambers{chambers} {}

	auto begin() -> void { log("serial ui started"); }

	auto tick(Timestamp now) -> void {
		checkForCommand(now);
		printStateWatch(now);
	}

   private:
	auto checkForCommand(Timestamp now) -> void {
		if (serial.available() > 0) {
			auto ch = static_cast<char>(serial.read());

			if (ch == '\n' || ch == '\r') {
				if (serial.peek() == '\n' || serial.peek() == '\r') {
					serial.read();
				}
				serial.println();
				processReadBuffer(now);
			} else {
				readBuffer += ch;
				if (echo) {
					serial.write(ch);
				}
			}
		}
	}

	auto processReadBuffer(Timestamp now) -> void {
		auto const clean =
			kev::remove_backspaces({readBuffer.c_str(), readBuffer.length()});
		auto const command = kev::trim(clean);
		processCommand(command, now);
		readBuffer.clear();
		serial.print("> ");
	}

	auto processCommand(string_view full_command, Timestamp now) -> void {
		auto const tokens = kev::tokens(full_command, ' ');
		if (tokens.empty() || tokens[0] == "") {
			serial.println("ok");
			return;
		}

		auto const command = tokens[0];
		if (command == "ping") {
			serial.println("pong");
		} else if (command == "echo") {
			echo = !echo;
			serial.printf("echo: %s\n", echo ? "on" : "off");
		} else if (command == "state" || command == "s") {
			stateCommand(tokens, now);
		} else if (command == "ui") {
			uiCommand(tokens, now);
		} else if (command == "force" || command == "f") {
			forceCommand(tokens);
		} else if (command == "unforce") {
			for (auto& ch : chambers) {
				ch.sensor.unforceTemp();
			}
		} else {
			log("unknown command: ", command);
		}
	}

	auto stateCommand(vector<string_view> const& tokens,
					  Timestamp now) -> void {
		if (tokens.size() == 1 || tokens[1] == "show") {
			showState(now);
			return;
		}

		auto const subcommand = tokens[1];

		if (subcommand == "watch") {
			stateWatch = true;
		} else if (subcommand == "unwatch") {
			stateWatch = false;
		} else {
			log("state: unknown subcommand: ", subcommand);
		}
	}

	auto showState(Timestamp now) -> void {
		serial.printf("state: %s\n", main.readStateStr());
		serial.printf("heater: %s\n", main.readHeater() ? "on" : "off");
		serial.printf("rotation: %s (%s)\n", main.readRotation() ? "on" : "off",
					  main.readRotationDir() ? "bw" : "fw");
		for (int i = 0; i < 3; ++i) {
			auto const temp = main.readTemp(i, now);
			if (!temp) {
				serial.printf("chamber %d: fan %s - temp ERROR\n", i + 1,
							  main.readFan(i) ? "on" : "off");
				continue;
			}
			serial.printf("chamber %d: fan %s - temp %.1f °C\n", i + 1,
						  main.readFan(i) ? "on" : "off", *temp);
		}

		auto c = main.getConfig();
		serial.printf("Config\n");
		serial.printf("  preheat temp: %.1f °C\n", c.preheatTemp);
		serial.printf("  chamber temp histeresis: %.1f °C\n",
					  c.chamberTempHist);

		auto stage1Mins =
			static_cast<int>(c.stages[0].duration.unsafeGetValue() / 1000 / 60);
		auto stage2Mins =
			static_cast<int>(c.stages[1].duration.unsafeGetValue() / 1000 / 60);
		auto stage3Mins =
			static_cast<int>(c.stages[2].duration.unsafeGetValue() / 1000 / 60);
		serial.printf("  stage 1 temp: %.1f °C - time: %dmin\n",
					  c.stages[0].temp, stage1Mins);
		serial.printf("  stage 2 temp: %.1f °C - time: %dmin\n",
					  c.stages[1].temp, stage2Mins);
		serial.printf("  stage 3 temp: %.1f °C - time: %dmin\n",
					  c.stages[2].temp, stage3Mins);

		serial.print("\n");
	}

	auto printStateWatch(kev::Timestamp now) -> void {
		if (readBuffer.length() != 0 || !stateWatch) {
			return;
		}

		if (stateWatchTimer.isDone(now)) {
			stateWatchTimer.reset(now);
			showState(now);
		}
	}

	// Simulate event from physical UI
	auto uiCommand(vector<string_view> const& tokens, Timestamp now) -> void {
		if (tokens.size() == 1) {
			log("ui: missing event");
			return;
		}

		auto const event = tokens[1];
		if (event == "preheat") {
			main.eventUiPreheat(now);
		} else {
			log("ui: unknown event: ", event);
		}
	}

	auto forceCommand(vector<string_view> const& tokens) -> void {
		if (tokens.size() < 2) {
			log("force: missing subcommand");
			return;
		}

		auto const subcommand = tokens[1];

		if (subcommand == "temp") {
			if (tokens.size() < 4) {
				log("force temp: usage: force temp <chamber> <temp>");
				return;
			}

			auto const chamberNo = kev::parse_int(tokens[2]);
			if (!chamberNo) {
				log("force temp: invalid chamber number");
				return;
			}
			if (*chamberNo <= 0 || *chamberNo > 3) {
				log("force temp: invalid chamber number");
				return;
			}

			auto const temp = kev::parse_int(tokens[3]);
			if (!temp) {
				log("force temp: invalid value");
				return;
			}

			chambers[*chamberNo - 1].sensor.forceTemp(*temp);
		} else {
			log("force: unknown subcommand: ", subcommand);
		}
	}

	String readBuffer;
	HardwareSerial& serial;
	kev::Log<> log{"serial"};
	bool echo = true;
	bool stateWatch = false;

	Timer stateWatchTimer{2_s};

	Main& main;
	chambers_t& chambers;
};
