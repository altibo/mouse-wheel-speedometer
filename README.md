# Mouse Wheel Speedometer

Project to measure the speed of a mouse running in a 30 cm diameter wheel using a reed switch.

## Versions

### NodeMCU (ESP8266) – Recommended
`nodemcu_mouse_speed.ino`

- WiFi Webserver with live graph (Chart.js)
- Statistics: current / max / mean speed, total distance, running time, activity by hour
- NTP time for hourly activity
- All-time max in EEPROM
- Piezo sound on records

**Wiring (NodeMCU):**
- Reed switch: D1 ↔ GND (INPUT_PULLUP)
- Piezo: D2 ↔ GND
- Power via USB or VIN

**Setup:**
1. Install ESP8266 board package in Arduino IDE
2. Board: NodeMCU 1.0 (ESP-12E Module)
3. Edit WiFi SSID/Password in the sketch
4. Upload
5. Open serial monitor for IP address, then browse to it

**Libraries:** Built-in (ESP8266WiFi, ESP8266WebServer, NTPClient)

### Arduino Pro Micro (legacy)
`mouse_speedometer.ino`

- 8x8 MAX7219 LED matrix display
- Piezo sounds
- See previous commit history for details

## Features (NodeMCU)

- Live speed graph
- Current, session max, all-time max, mean speed
- Total distance and running time
- Activity histogram by hour of day
- Automatic session detection

Wheel circumference: π × 0.3 m
