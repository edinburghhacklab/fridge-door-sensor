/*
 * Copyright 2025  Simon Arlott
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <Arduino.h>
#include <esp_timer.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <string>

#if __has_include("config.h")
# include "config.h"
#else
# include "config.h.example"
#endif

static constexpr uint64_t ONE_S = 1000 * 1000ULL;
static constexpr unsigned int DOOR_GPIO = 14;
static uint64_t open_at_us = 0;
static uint64_t open_s = 0;
static uint64_t last_open_s = 0;
static uint64_t last_publish_us = 0;
static bool door_open = false;

static uint64_t last_wifi_us = 0;
static bool wifi_up = false;

static uint64_t last_mqtt_us = 0;

static WiFiClient client;
static PubSubClient mqtt(client);
static String device_id;

void setup() {
	pinMode(DOOR_GPIO, INPUT_PULLDOWN);

	device_id = String("fridge-door-sensor_") + String(ESP.getEfuseMac(), HEX);

	WiFi.persistent(false);
	WiFi.setAutoReconnect(false);
	WiFi.setSleep(false);
	WiFi.mode(WIFI_STA);

	mqtt.setServer(MQTT_HOSTNAME, MQTT_PORT);
	mqtt.setCallback([] (char*, uint8_t*, unsigned int) {
		mqtt.publish("meta/mqtt-agents/reply", device_id.c_str());
	});
}

void loop() {
	if (digitalRead(DOOR_GPIO) == LOW) {
		if (!door_open) {
			door_open = true;
			open_at_us = esp_timer_get_time();
		}
		open_s = (esp_timer_get_time() - open_at_us) / 1000000ULL;
	} else if (door_open) {
		door_open = false;
		open_s = 0;
	}

	switch (WiFi.status()) {
	case WL_IDLE_STATUS:
	case WL_NO_SSID_AVAIL:
	case WL_CONNECT_FAILED:
	case WL_CONNECTION_LOST:
	case WL_DISCONNECTED:
		if (!last_wifi_us || wifi_up || esp_timer_get_time() - last_wifi_us > 30 * ONE_S) {
			Serial.println("WiFi reconnect");
			WiFi.disconnect();
			WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
			last_wifi_us = esp_timer_get_time();
			wifi_up = false;
		}
		break;

	case WL_CONNECTED:
		if (!wifi_up) {
			Serial.println("WiFi connected");
			wifi_up = true;
		}
		break;

	case WL_NO_SHIELD:
	case WL_SCAN_COMPLETED:
		break;
	}

	mqtt.loop();

	if (wifi_up) {
		if (!mqtt.connected() && (!last_mqtt_us || esp_timer_get_time() - last_mqtt_us > ONE_S)) {
			Serial.println("MQTT connecting");
			mqtt.connect(device_id.c_str());

			if (mqtt.connected()) {
				Serial.println("MQTT connected");
				mqtt.subscribe("meta/mqtt-agents/poll");
				mqtt.publish("meta/mqtt-agents/announce", device_id.c_str());
				mqtt.publish(MQTT_TOPIC, String(open_s).c_str());
				last_publish_us = esp_timer_get_time();
			} else {
				Serial.println("MQTT connection failed");
				last_mqtt_us = esp_timer_get_time();
			}
		}
	}

	if (last_open_s != open_s || esp_timer_get_time() - last_publish_us > 60 * ONE_S) {
		last_open_s = open_s;

		if (open_s == 0) {
			Serial.println("Closed");
		} else {
			Serial.print("Open for ");
			Serial.print(open_s);
			Serial.println("s");
		}

		if (wifi_up && mqtt.connected()) {
			mqtt.publish(MQTT_TOPIC, String(open_s).c_str());
		}
		last_publish_us = esp_timer_get_time();
	}
}
