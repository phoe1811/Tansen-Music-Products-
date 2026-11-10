
#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <DFRobotDFPlayerMini.h>

#define SIMULATION_MODE true
#define DHT_PIN 4
#define DHT_TYPE DHT11
#define DF_RX_PIN 16
#define DF_TX_PIN 17

const char* AP_SSID = "tkgod11";
const char* AP_PASSWORD = "@143chunks";

DHT dht(DHT_PIN, DHT_TYPE);
HardwareSerial mp3Serial(2);
DFRobotDFPlayerMini player;
WebServer server(80);

float temperature = 26.0;
float humidity = 60.0;

bool automaticMode = true;
bool playing = false;
bool playerReady = false;

int volumeLevel = 20;
int currentTrack = 1;

String mood = "calm";

unsigned long lastSensorRead = 0;
unsigned long lastMoodChange = 0;

const unsigned long SENSOR_INTERVAL = 2500;
const unsigned long MOOD_COOLDOWN = 15000;

int trackForMood(const String& value) {
  if (value == "rainy") return 1;
  if (value == "energetic") return 2;
  return 3;
}
String determineMood() {
  if (humidity >= 75.0) return "rainy";
  if (temperature >= 30.0) return "energetic";
  return "calm";
}
void playTrack(int track) {
  currentTrack = constrain(track, 1, 3);
  playing = true;
  if (playerReady) {
    player.playMp3Folder(currentTrack);
  }
}
void applyMood(bool force = false) {
  String desired = determineMood();

  if (!automaticMode || desired == mood) return;

  if (!force &&
      millis() - lastMoodChange < MOOD_COOLDOWN) {
    return;
  }
  mood = desired;
  lastMoodChange = millis();
  playTrack(trackForMood(mood));
}
String jsonState() {
  String json = "{";
  json += "\"temperature\":" +
          String(temperature, 1) + ",";

  json += "\"humidity\":" +
          String(humidity, 1) + ",";
  json += "\"mood\":\"" + mood + "\",";
  json += "\"automatic\":";
  json += automaticMode ? "true," : "false,";
  json += "\"playing\":";
  json += playing ? "true," : "false,";
  json += "\"track\":" +
          String(currentTrack) + ",";
  json += "\"volume\":" +
          String(volumeLevel) + ",";
  json += "\"simulation\":";
  json += SIMULATION_MODE ? "true" : "false";
  json += "}";
  return json;
}
const char PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport"
content="width=device-width,initial-scale=1">
<title>AtmosBeat</title>
<style>
* {
  box-sizing: border-box;
}
body {
  margin: 0;
  background: #0b1220;
  color: #f1f5f9;
  font-family: Arial, sans-serif;
}
main {
  max-width: 540px;
  margin: 30px auto;
  padding: 20px;
}
h1 {
  color: #7dd3fc;
}
.panel {
  background: #19263b;
  border: 1px solid #304159;
  border-radius: 18px;
  padding: 20px;
  margin: 16px 0;
}
.stats {
  display: flex;
  gap: 12px;
}
.stats .panel {
  flex: 1;
  min-width: 0;
}

button {
  background: #38bdf8;
  color: #08121e;
  border: none;
  border-radius: 10px;
  padding: 12px 15px;
  margin: 4px;
  cursor: pointer;
  font-weight: bold;
}
input[type="range"] {
  width: 100%;
}
.muted {
  color: #a6b6ca;
}
</style>
</head>
<body>
<main>
<h1>AtmosBeat</h1>
<p class="muted">Your weather. Your music.</p>
<div class="stats">
  <div class="panel">
    <p>Temperature</p>
    <h2 id="temp">--</h2>
  </div>
  <div class="panel">
    <p>Humidity</p>
    <h2 id="humidity">--</h2>
  </div>
</div>
<div class="panel">
  <p class="muted">Current Mood</p>
  <h2 id="mood">Loading...</h2>
  <p id="track"></p>
  <p id="mode"></p>
  <p id="sim"></p>
</div>
<div class="panel">
  <button onclick="send('toggle')" id="play">
    Play / Pause
  </button>
  <button onclick="send('previous')">
    Previous
  </button>
  <button onclick="send('next')">
    Next
  </button>
  <p>
    <button onclick="send('auto')">
      Auto Mode
    </button>
    <button onclick="send('manual')">
      Manual Mode
    </button>
  </p>
  <label for="volume">Volume</label>
  <input
    id="volume"
    type="range"
    min="0"
    max="30"
    value="20"
    onchange="send('volume',this.value)"
  >
</div>
<p class="muted" id="status">
Connecting...
</p>
</main>
<script>
async function refresh() {
  try {
    const response = await fetch('/api/state');
    if (!response.ok) {
      throw Error('Network error');
    }
    const s = await response.json();
    document.getElementById('temp').textContent =
      s.temperature + ' °C';
    document.getElementById('humidity').textContent =
      s.humidity + ' %';
    document.getElementById('mood').textContent =
      s.mood.toUpperCase();
    document.getElementById('track').textContent =
      'Track ' + s.track;
    document.getElementById('mode').textContent =
      s.automatic ? 'Automatic Mode' : 'Manual Mode';
    document.getElementById('sim').textContent =
      s.simulation ? 'Simulation Mode' : 'Hardware Mode';
    document.getElementById('play').textContent =
      s.playing ? 'Pause' : 'Play';
    const volume = document.getElementById('volume');
    if (document.activeElement !== volume) {
      volume.value = s.volume;
    }
    document.getElementById('status').textContent =
      'Connected';
  } catch (error) {
    document.getElementById('status').textContent =
      'Connection unavailable';
  }
}
async function send(action, value) {
  try {
    let url = '/api/control?action=' +
              encodeURIComponent(action);
    if (value !== undefined) {
      url += '&value=' + encodeURIComponent(value);
    }
    const response = await fetch(url, {
      method: 'POST'
    });
    if (!response.ok) {
      throw Error('Command failed');
    }
    await refresh();
  } catch (error) {
    document.getElementById('status').textContent =
      error.message;
  }
}
setInterval(refresh, 2500);
refresh();
</script>
</body>
</html>
)HTML";
void handleControl() {
  String action = server.arg("action");
  if (action == "toggle") {
    playing = !playing;
    if (playerReady) {
      if (playing) {
        player.start();
      } else {
        player.pause();
      }
    }
  }
  else if (action == "auto") {
    automaticMode = true;
    applyMood(true);
  }
  else if (action == "manual") {
    automaticMode = false;
  }
  else if (action == "next" ||
           action == "previous") {
    automaticMode = false;
    int nextTrack = currentTrack +
      (action == "next" ? 1 : -1);
    if (nextTrack > 3) nextTrack = 1;
    if (nextTrack < 1) nextTrack = 3;
    playTrack(nextTrack);
  }
  else if (action == "volume") {
    if (!server.hasArg("value")) {
      server.send(400, "text/plain",
                  "Missing value");
      return;
    }
    volumeLevel = constrain(
      server.arg("value").toInt(), 0, 30
    );
    if (playerReady) {
      player.volume(volumeLevel);
    }
  }
  else {
    server.send(400, "text/plain",
                "Unknown action");
    return;
  }
  server.send(
    200,
    "application/json",
    jsonState()
  );
}
void readSensors() {
  if (millis() - lastSensorRead <
      SENSOR_INTERVAL) {
    return;
  }
  lastSensorRead = millis();
#if SIMULATION_MODE
  unsigned long phase =
    (millis() / 20000UL) % 3;
  if (phase == 0) {
    temperature = 24.0;
    humidity = 82.0;
  }

  else if (phase == 1) {
    temperature = 34.0;
    humidity = 38.0;
  }
  else {
    temperature = 22.0;
    humidity = 55.0;
  }
#else
  float newHumidity = dht.readHumidity();
  float newTemperature = dht.readTemperature();
  if (!isnan(newHumidity) &&
      !isnan(newTemperature)) {
    humidity = newHumidity;
    temperature = newTemperature;
  } else {
    Serial.println("DHT11 reading failed");
  }
#endif
  applyMood();
}
void setup() {
  Serial.begin(115200);
#if !SIMULATION_MODE
  dht.begin();
  mp3Serial.begin(
    9600,
    SERIAL_8N1,
    DF_RX_PIN,
    DF_TX_PIN
  );
  playerReady = player.begin(
    mp3Serial, true, true
  );
  if (playerReady) {
    player.volume(volumeLevel);
  } else {
    Serial.println("DFPlayer not detected");
  }
#endif
  WiFi.mode(WIFI_AP);

  if (!WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    Serial.println("Wi-Fi AP failed");
  }
  Serial.print("AtmosBeat URL: http://");
  Serial.println(WiFi.softAPIP());
  server.on("/", HTTP_GET, []() {
    server.send_P(
      200,
      "text/html",
      PAGE
    );
  });
  server.on("/api/state", HTTP_GET, []() {
    server.send(
      200,
      "application/json",
      jsonState()
    );
  });
  server.on(
    "/api/control",
    HTTP_POST,
    handleControl
  );
  server.onNotFound([]() {
    server.send(
      404,
      "text/plain",
      "Not found"
    );
  });

  server.begin();
  mood = determineMood();
  currentTrack = trackForMood(mood);
  lastMoodChange = millis();
  Serial.println("tkgod11 ready");
}
void loop() {
  server.handleClient();
  readSensors();
}
