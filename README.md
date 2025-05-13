# fridge-door-sensor

## Overview
Monitor fridge door status using an ESP32-S2.

## Dependencies
[PlatformIO](https://platformio.org/)

## Build
`platformio run`

## Install
`platformio run -t upload`

## Hardware interface
The state is monitored using GPIO 14, which must be low when the door is closed.

## Network interface
Copy `src/config.h.example` to `src/config.h` to configure the WiFi network,
MQTT hostname and MQTT topic (e.g. `sensor/g1/fridge`).
