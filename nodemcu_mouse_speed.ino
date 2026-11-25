#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <EEPROM.h>

#define REED_PIN D1
#define PIEZO_PIN D2

const char* ssid = "DEIN_WIFI_SSID";
const char* password = "DEIN_WIFI_PASSWORT";

const float WHEEL_DIAMETER = 0.3; // m
const float WHEEL_CIRC = WHEEL_DIAMETER * PI;
const unsigned long DEBOUNCE_US = 50000;

ESP8266WebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600, 60000); // CET offset

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

const int MAX_SAMPLES = 100;
float speedHistory[MAX_SAMPLES];
unsigned long timeHistory[MAX_SAMPLES];
int sampleCount = 0;
int sampleIndex = 0;

int hourlyActivity[24] = {0};

void setup() {
  Serial.begin(115200);
  pinMode(REED_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(REED_PIN), reedISR, FALLING);
  pinMode(PIEZO_PIN, OUTPUT);

  EEPROM.begin(512);
  EEPROM.get(0, allTimeMax);
  if (isnan(allTimeMax) || allTimeMax < 0) allTimeMax = 0.0;

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  timeClient.begin();
  timeClient.update();

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
  timeClient.update();

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
      int hour = timeClient.getHours();
      if (hour >= 0 && hour < 24) hourlyActivity[hour]++;
    }
  }

  // Stop running detection after timeout
  if (isRunning && (millis() - lastPulseTime / 1000 > 5000)) {
    isRunning = false;
    totalRunningTimeMs += millis() - sessionStart;
  }

  delay(10);
}

void addSample(float speed) {
  speedHistory[sampleIndex] = speed;
  timeHistory[sampleIndex] = timeClient.getEpochTime();
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
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
  <style>
    body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }
    .card { background: white; padding: 15px; margin: 10px 0; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
    h1 { color: #333; }
    .stats { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 10px; }
    .stat { text-align: center; }
    .value { font-size: 1.8em; font-weight: bold; color: #007bff; }
  </style>
</head>
<body>
  <h1>Mausrad Geschwindigkeit</h1>
  <div class="card">
    <canvas id="speedChart" height="100"></canvas>
  </div>
  <div class="card stats" id="stats"></div>
  <div class="card">
    <h3>Aktivität nach Uhrzeit</h3>
    <canvas id="hourChart" height="80"></canvas>
  </div>
  <script>
    const speedCtx = document.getElementById('speedChart').getContext('2d');
    const speedChart = new Chart(speedCtx, {
      type: 'line',
      data: { labels: [], datasets: [{ label: 'Geschwindigkeit km/h', data: [], borderColor: '#007bff', tension: 0.3 }] },
      options: { scales: { y: { beginAtZero: true } } }
    });

    const hourCtx = document.getElementById('hourChart').getContext('2d');
    const hourChart = new Chart(hourCtx, {
      type: 'bar',
      data: { labels: Array.from({length:24}, (_,i)=>i+':00'), datasets: [{ label: 'Aktivität', data: [], backgroundColor: '#28a745' }] },
      options: { scales: { y: { beginAtZero: true } } }
    });

    async function update() {
      const dataRes = await fetch('/data');
      const data = await dataRes.json();
      speedChart.data.labels = data.labels;
      speedChart.data.datasets[0].data = data.speeds;
      speedChart.update();

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
      hourChart.data.datasets[0].data = stats.hourly;
      hourChart.update();
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
  json += "\"runningMin\":" + String(totalRunningTimeMs / 60000.0, 1) + ",";
  json += "\"hourly\":[";
  for (int i = 0; i < 24; i++) {
    if (i > 0) json += ",";
    json += String(hourlyActivity[i]);
  }
  json += "]}";
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
