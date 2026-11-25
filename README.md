# Mouse Wheel Speedometer

Project to measure the speed of a mouse running in a 30 cm diameter wheel using a reed switch.

## Versions

### NodeMCU (ESP8266) – Recommended
`nodemcu_mouse_speed.ino`

- Eigener versteckter Hotspot (kein Internet nötig)
- Webserver mit Live-Graph (kein externes JS)
- Statistik: aktuell, Max, Mittel, Distanz, Laufzeit
- All-Time-Max in EEPROM
- Piezo bei Rekord

**Wiring (NodeMCU):**
- Reed switch: D1 ↔ GND (INPUT_PULLUP)
- Piezo: D2 ↔ GND
- Power via USB oder VIN

**Setup:**
1. ESP8266 Board Package installieren
2. Board: NodeMCU 1.0 (ESP-12E Module)
3. Upload `nodemcu_mouse_speed.ino`
4. Mit dem versteckten WLAN "Mausrad" (Passwort: mausrad123) verbinden
5. Browser öffnen: http://192.168.4.1

**Libraries:** Nur Standard (ESP8266WiFi, ESP8266WebServer)

### Arduino Pro Micro (legacy)
`mouse_speedometer.ino`

- 8x8 MAX7219 LED-Matrix
- Siehe ältere Commits

## Features (NodeMCU)

- Live-Geschwindigkeitsgraph
- Aktuell, Session-Max, All-Time-Max, Mittel
- Gesamtdistanz und Laufzeit
- Session-Erkennung

Radumfang: π × 0.3 m
