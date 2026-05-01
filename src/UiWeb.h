#pragma once

#include <WebServer.h>
#include <WiFi.h>

#include "Main.h"
#include "kev/Log.h"
#include "kev/String.h"
#include "kev/Time.h"

using std::string_view;
using std::vector;
using chambers_t = std::array<Chamber, 3>;

template <typename = void>
struct UiWebImpl {
	UiWebImpl(Main& main, chambers_t& chambers)
		: server(80), main(main), chambers(chambers) {}

	void begin() {
		log("connecting to wifi...");

		WiFi.mode(WIFI_STA);
		WiFi.begin("YOUR_SSID", "YOUR_PASSWORD");

		log.partial_start();
		while (WiFi.status() != WL_CONNECTED) {
			delay(500);
			log.partial(".");
		}
		log.partial_end();

		log("wifi connected");
		log("ip: ", WiFi.localIP().toString().c_str());

		server.on("/", [this]() { handleRoot(); });
		server.on("/cmd", [this]() { handleCmd(); });
		server.on("/state", [this]() { handleState(); });

		server.begin();
		log("web ui started");
	}

	void tick(kev::Timestamp now) {
		this->now = now;
		server.handleClient();
	}

   private:
	// ---------------- HTTP HANDLERS ----------------

	void handleRoot() {
		server.send(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 UI</title>
    <style>
        body { font-family: monospace; background: #111; color: #0f0; }
        button { margin: 5px; padding: 10px; }
        #log { white-space: pre; }
    </style>
</head>
<body>
    <h2>ESP32 Control</h2>

    <button onclick="cmd('ping')">Ping</button>
    <button onclick="cmd('ui start')">Start</button>
    <button onclick="cmd('ui stop')">Stop</button>
    <button onclick="cmd('ui preheat')">Preheat</button>

    <button onclick="cmd('state')">State</button>

    <pre id="log"></pre>

<script>
function cmd(c) {
    fetch('/cmd?c=' + encodeURIComponent(c))
        .then(r => r.text())
        .then(t => log(t));
}

function log(t) {
    document.getElementById('log').textContent += t + "\n";
}

setInterval(() => {
    fetch('/state')
        .then(r => r.json())
        .then(s => {
            log(JSON.stringify(s, null, 2));
        });
}, 5000);
</script>
</body>
</html>
)rawliteral");
	}

	void handleCmd() {
		if (!server.hasArg("c")) {
			server.send(400, "text/plain", "missing cmd");
			return;
		}

		auto cmd = server.arg("c");
		auto result = processCommand(cmd);

		server.send(200, "text/plain", result);
	}

	void handleState() {
		String json = "{";

		json += "\"state\":\"" + String(main.readStateStr()) + "\",";
		json +=
			"\"heater\":" + String(main.readHeater(now) ? "true" : "false") +
			",";
		json +=
			"\"rotation\":" + String(main.readRotation() ? "true" : "false") +
			",";

		json += "\"chambers\":[";
		for (int i = 0; i < 3; ++i) {
			auto temp = main.readTemp(i, now);

			json += "{";
			json +=
				"\"fan\":" + String(main.readFan(i) ? "true" : "false") + ",";
			if (temp) {
				json += "\"temp\":" + String(*temp);
			} else {
				json += "\"temp\":null";
			}
			json += "}";

			if (i != 2)
				json += ",";
		}
		json += "]";

		json += "}";

		server.send(200, "application/json", json);
	}

	// ---------------- COMMAND LOGIC ----------------

	String processCommand(String const& full) {
		auto clean = kev::trim(full.c_str());
		auto tokens = kev::tokens(clean, ' ');

		if (tokens.empty() || tokens[0] == "") {
			return "ok";
		}

		auto const command = tokens[0];

		if (command == "ping") {
			return "pong";
		} else if (command == "state" || command == "s") {
			return buildStateString();
		} else if (command == "ui") {
			return handleUi(tokens);
		} else if (command == "force" || command == "f") {
			return handleForce(tokens);
		}

		return "unknown command";
	}

	String handleUi(vector<string_view> const& tokens) {
		if (tokens.size() < 2)
			return "missing ui event";

		auto e = tokens[1];

		if (e == "preheat")
			main.eventUiPreheat(now);
		else if (e == "start")
			main.eventUiStart(now);
		else if (e == "stop")
			main.eventUiStop(now);
		else if (e == "pause")
			main.eventUiPause(now);
		else if (e == "rfw")
			main.eventUiRotateFw();
		else if (e == "rfw_stop")
			main.eventUiRotateFwStop();
		else if (e == "rbw")
			main.eventUiRotateBw();
		else if (e == "rbw_stop")
			main.eventUiRotateBwStop();
		else
			return "unknown ui event";

		return "ok";
	}

	String handleForce(vector<string_view> const& tokens) {
		if (tokens.size() < 4)
			return "usage: force temp <chamber> <temp>";

		auto chamberNo = kev::parse_int(tokens[2]);
		auto temp = kev::parse_int(tokens[3]);

		if (!chamberNo || !temp)
			return "invalid args";

		chambers[*chamberNo - 1].sensor.forceTemp(*temp);
		return "ok";
	}

	String buildStateString() {
		String out;

		out += "state: ";
		out += main.readStateStr();
		out += "\n";

		out += "heater: ";
		out += (main.readHeater(now) ? "on\n" : "off\n");

		for (int i = 0; i < 3; ++i) {
			auto temp = main.readTemp(i, now);
			out += "chamber ";
			out += i + 1;
			out += ": ";

			if (temp) {
				out += String(*temp) + " C\n";
			} else {
				out += "ERROR\n";
			}
		}

		return out;
	}

	// ---------------- MEMBERS ----------------

	WebServer server;
	kev::Log<> log{"web"};

	Main& main;
	chambers_t& chambers;

	kev::Timestamp now{};
};

using UiWeb = UiWebImpl<>;
