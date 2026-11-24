#include <LedControl.h>
#include <EEPROM.h>

#define DIN_PIN 16
#define CLK_PIN 15
#define CS_PIN 10
#define REED_PIN 2
#define PIEZO_PIN 9

LedControl lc = LedControl(DIN_PIN, CLK_PIN, CS_PIN, 1);

const float WHEEL_DIAMETER = 0.3; // meters
const float WHEEL_CIRC = WHEEL_DIAMETER * PI;
const unsigned long DEBOUNCE_US = 50000;

volatile unsigned long lastPulseTime = 0;
volatile unsigned long pulseInterval = 0;
volatile bool pulseDetected = false;

float currentSpeed = 0.0;
float dailyRecord = 0.0;
float allTimeRecord = 0.0;

int displayMode = 0; // 0: current, 1: daily, 2: alltime
unsigned long lastModeSwitch = 0;
const unsigned long MODE_INTERVAL = 3000;

byte digitPatterns[10][8] = {
  {B00111100, B01111110, B11000011, B11000011, B11000011, B11000011, B01111110, B00111100}, // 0
  {B00011000, B00111000, B00011000, B00011000, B00011000, B00011000, B00111100, B00111100}, // 1
  {B01111100, B11111110, B00000110, B00001100, B00011000, B00110000, B11111110, B11111110}, // 2
  {B01111100, B11111110, B00000110, B00111100, B00000110, B00000110, B11111110, B01111100}, // 3
  {B00001100, B00011100, B00111100, B01101100, B11111110, B00001100, B00001100, B00001100}, // 4
  {B11111110, B11111110, B11000000, B11111100, B00000110, B00000110, B11111110, B01111100}, // 5
  {B00111100, B01111110, B11000000, B11111100, B11000110, B11000110, B01111110, B00111100}, // 6
  {B11111110, B11111110, B00000110, B00001100, B00011000, B00110000, B00110000, B00110000}, // 7
  {B00111100, B01111110, B11000110, B01111100, B11000110, B11000110, B01111110, B00111100}, // 8
  {B00111100, B01111110, B11000110, B01111110, B00000110, B00000110, B01111110, B00111100}  // 9
};

void setup() {
  pinMode(REED_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(REED_PIN), reedISR, FALLING);
  pinMode(PIEZO_PIN, OUTPUT);

  lc.shutdown(0, false);
  lc.setIntensity(0, 8);
  lc.clearDisplay(0);

  EEPROM.get(0, allTimeRecord);
  if (isnan(allTimeRecord) || allTimeRecord < 0) allTimeRecord = 0.0;
  dailyRecord = 0.0;

  playStartupSound();
}

void reedISR() {
  unsigned long now = micros();
  if (now - lastPulseTime > DEBOUNCE_US) {
    pulseInterval = now - lastPulseTime;
    lastPulseTime = now;
    pulseDetected = true;
  }
}

void loop() {
  if (pulseDetected) {
    pulseDetected = false;
    if (pulseInterval > 0) {
      float revPerSec = 1000000.0 / pulseInterval;
      currentSpeed = revPerSec * WHEEL_CIRC * 3.6; // km/h
      if (currentSpeed > dailyRecord) {
        dailyRecord = currentSpeed;
        playRecordSound(false);
      }
      if (currentSpeed > allTimeRecord) {
        allTimeRecord = currentSpeed;
        EEPROM.put(0, allTimeRecord);
        playRecordSound(true);
      }
    }
  }

  if (millis() - lastModeSwitch > MODE_INTERVAL) {
    displayMode = (displayMode + 1) % 3;
    lastModeSwitch = millis();
  }

  updateDisplay();
  delay(50);
}

void updateDisplay() {
  lc.clearDisplay(0);
  float value = 0;
  switch (displayMode) {
    case 0: value = currentSpeed; break;
    case 1: value = dailyRecord; break;
    case 2: value = allTimeRecord; break;
  }
  int displayVal = (int)(value + 0.5); // round to int
  if (displayVal > 99) displayVal = 99;
  if (displayVal < 0) displayVal = 0;
  int tens = displayVal / 10;
  int units = displayVal % 10;
  showDigit(tens, 0);
  showDigit(units, 4);
}

void showDigit(int digit, int colOffset) {
  if (digit < 0 || digit > 9) return;
  for (int row = 0; row < 8; row++) {
    byte pattern = digitPatterns[digit][row];
    for (int col = 0; col < 4; col++) {
      bool ledOn = bitRead(pattern, 7 - col);
      lc.setLed(0, row, col + colOffset, ledOn);
    }
  }
}

void playStartupSound() {
  tone(PIEZO_PIN, 523, 100);
  delay(150);
  tone(PIEZO_PIN, 659, 100);
  delay(150);
  tone(PIEZO_PIN, 784, 200);
  delay(250);
  noTone(PIEZO_PIN);
}

void playRecordSound(bool isAllTime) {
  if (isAllTime) {
    for (int i = 0; i < 3; i++) {
      tone(PIEZO_PIN, 880, 150);
      delay(200);
      tone(PIEZO_PIN, 1046, 150);
      delay(200);
    }
  } else {
    tone(PIEZO_PIN, 659, 200);
    delay(250);
    tone(PIEZO_PIN, 784, 200);
    delay(250);
    tone(PIEZO_PIN, 880, 300);
    delay(350);
  }
  noTone(PIEZO_PIN);
}
