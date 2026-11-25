#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

#define REED_PIN D1
#define PIEZO_PIN D2

const char* ap_ssid = "Mausrad";
const char* ap_password = "mausrad123"; // min 8 chars

const float WHEEL_DIAMETER = 0.3; // m
const float WHEEL_CIRC = WHEEL_DIAMETER * PI;
const unsigned long DEBOUNCE_US = 50000;

ESP8266WebServer server(80);

volatile unsigned long lastPulseTime = 0;
volatile unsigned long pulseInterval = 0;
volatile bool pulseDetected = false;

float currentSpeed = 0.0;
float maxSpeed = 0.0;
float allTimeMax = 0.0;
float totalDistance = 0.0;
unsigned long totalRunningTimeMs = 0;
unsigned long sessionStart = 0;
bool isRunning = false;

const int MAX_SAMPLES = 60;
float speedHistory[MAX_SAMPLES];
unsigned long timeHistory[MAX_SAMPLES]; // relative seconds
int sampleCount = 0;
int sampleIndex = 0;
unsigned long startMillis = 0;

void setup() {
  Serial.begin(115200);
  pinMode(REED_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(REED_PIN), reedISR, FALLING);
  pinMode(PIEZO_PIN, OUTPUT);

  EEPROM.begin(512);
  EEPROM.get(0, allTimeMax);
  if (isnan(allTimeMax) || allTimeMax < 0) allTimeMax = 0.0;

  // Hidden SoftAP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password, 1, 1); // channel 1, hidden=1
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  startMillis = millis();

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/stats", handleStats);
  server.begin();

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
  server.handleClient();

  if (pulseDetected) {
    pulseDetected = false;
    if (pulseInterval > 0) {
      float revPerSec = 1000000.0 / pulseInterval;
      currentSpeed = revPerSec * WHEEL_CIRC * 3.6;
      totalDistance += WHEEL_CIRC;

      if (!isRunning) {
        isRunning = true;
        sessionStart = millis();
      }

      if (currentSpeed > maxSpeed) maxSpeed = currentSpeed;
      if (currentSpeed > allTimeMax) {
        allTimeMax = currentSpeed;
        EEPROM.put(0, allTimeMax);
        EEPROM.commit();
        playRecordSound(true);
      }

      addSample(currentSpeed);
    }
  }

  if (isRunning && (millis() - (lastPulseTime / 1000) > 5000)) {
    isRunning = false;
    totalRunningTimeMs += millis() - sessionStart;
  }

  delay(10);
}

void addSample(float speed) {
  speedHistory[sampleIndex] = speed;
  timeHistory[sampleIndex] = (millis() - startMillis) / 1000;
  sampleIndex = (sampleIndex + 1) % MAX_SAMPLES;
  if (sampleCount < MAX_SAMPLES) sampleCount++;
}

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Mausrad Speedometer</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }
    .card { background: white; padding: 15px; margin: 10px 0; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
    h1 { color: #333; }
    .stats { display: grid; grid-template-columns: repeat(auto-fit, minmax(140px, 1fr)); gap: 10px; }
    .stat { text-align: center; }
    .value { font-size: 1.6em; font-weight: bold; color: #007bff; }
    canvas { width: 100%; height: 200px; background: #fff; border: 1px solid #ddd; }
  </style>
</head>
<body>
  <h1>Mausrad Geschwindigkeit</h1>
  <div class="card">
    <canvas id="speedChart"></canvas>
  </div>
  <div class="card stats" id="stats"></div>
  <script>
    const canvas = document.getElementById('speedChart');
    const ctx = canvas.getContext('2d');

    function drawChart(labels, speeds) {
      const w = canvas.width = canvas.offsetWidth;
      const h = canvas.height = 200;
      ctx.clearRect(0, 0, w, h);
      if (speeds.length < 2) return;

      const maxS = Math.max(...speeds, 1);
      ctx.beginPath();
      ctx.strokeStyle = '#007bff';
      ctx.lineWidth = 2;
      for (let i = 0; i < speeds.length; i++) {
        const x = (i / (speeds.length - 1)) * w;
        const y = h - (speeds[i] / maxS) * (h - 20) - 10;
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      }
      ctx.stroke();

      ctx.fillStyle = '#666';
      ctx.font = '12px Arial';
      ctx.fillText('0', 5, h - 5);
      ctx.fillText(maxS.toFixed(1) + ' km/h', 5, 15);
    }

    async function update() {
      const dataRes = await fetch('/data');
      const data = await dataRes.json();
      drawChart(data.labels, data.speeds);

      const statsRes = await fetch('/stats');
      const stats = await statsRes.json();
      document.getElementById('stats').innerHTML = `
        <div class="stat"><div>Aktuell</div><div class="value">${stats.current.toFixed(1)} km/h</div></div>
        <div class="stat"><div>Max (Session)</div><div class="value">${stats.max.toFixed(1)} km/h</div></div>
        <div class="stat"><div>All-Time Max</div><div class="value">${stats.allTime.toFixed(1)} km/h</div></div>
        <div class="stat"><div>Mittel</div><div class="value">${stats.mean.toFixed(1)} km/h</div></div>
        <div class="stat"><div>Distanz</div><div class="value">${stats.distance.toFixed(1)} m</div></div>
        <div class="stat"><div>Laufzeit</div><div class="value">${Math.round(stats.runningMin)} min</div></div>
      `;
    }
    setInterval(update, 2000);
    update();
  </script>
</body>
</html>
)rawliteral";
  server.send(200, "text/html", html);
}

void handleData() {
  String json = "{\"labels\":[";
  String speeds = "],\"speeds\":[";
  for (int i = 0; i < sampleCount; i++) {
    int idx = (sampleIndex - sampleCount + i + MAX_SAMPLES) % MAX_SAMPLES;
    if (i > 0) { json += ","; speeds += ","; }
    json += String(timeHistory[idx]);
    speeds += String(speedHistory[idx], 1);
  }
  json += speeds + "]}";
  server.send(200, "application/json", json);
}

void handleStats() {
  float sum = 0;
  for (int i = 0; i < sampleCount; i++) {
    int idx = (sampleIndex - sampleCount + i + MAX_SAMPLES) % MAX_SAMPLES;
    sum += speedHistory[idx];
  }
  float mean = sampleCount > 0 ? sum / sampleCount : 0;

  String json = "{";
  json += "\"current\":" + String(currentSpeed, 1) + ",";
  json += "\"max\":" + String(maxSpeed, 1) + ",";
  json += "\"allTime\":" + String(allTimeMax, 1) + ",";
  json += "\"mean\":" + String(mean, 1) + ",";
  json += "\"distance\":" + String(totalDistance, 1) + ",";
  json += "\"runningMin\":" + String(totalRunningTimeMs / 60000.0, 1);
  json += "}";
  server.send(200, "application/json", json);
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
  }
  noTone(PIEZO_PIN);
}
