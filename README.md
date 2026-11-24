# Mouse Wheel Speedometer

Arduino Pro Micro (ATmega32U4) project that measures the speed of a mouse running in a 30 cm diameter wheel using a reed switch, displays current speed / daily record / all-time record on an 8x8 LED matrix (MAX7219), and plays piezo sounds on new records.

## Hardware

- Arduino Pro Micro
- MAX7219 8x8 LED Matrix module (pins: VCC, GND, DIN, CS, CLK)
- Piezo buzzer
- Reed switch (on the wheel, one pulse per revolution)
- Mouse wheel, 30 cm diameter

## Wiring

### LED Matrix (MAX7219)
- VCC → Pro Micro VCC (5V)
- GND → Pro Micro GND
- DIN → Pin 16 (MOSI)
- CLK → Pin 15 (SCK)
- CS  → Pin 10

### Reed Switch
- One side → Pin 2
- Other side → GND
(Uses internal pull-up. Magnet on the wheel triggers the switch once per revolution.)

### Piezo Buzzer
- Positive → Pin 9
- Negative → GND

## Libraries

Install via Arduino IDE Library Manager:
- LedControl (by Eberhard Fahle)

## Features

- Calculates speed in km/h from pulse interval (circumference = π * 0.3 m)
- Cycles display every 3 s: current speed → daily record → all-time record
- Shows two-digit integer speed (rounded)
- Daily record resets on power cycle
- All-time record stored in EEPROM
- Piezo melody on new daily or all-time record
- Startup sound

## Upload

1. Select board: Arduino Pro Micro (or Leonardo)
2. Select correct processor/port
3. Upload `mouse_speedometer.ino`

## Notes

- Debounce: 50 ms minimum between pulses
- Max displayed speed: 99 km/h
- Adjust pins in the sketch if needed
- For better digit display or patterns, modify `digitPatterns` and `showDigit()`
