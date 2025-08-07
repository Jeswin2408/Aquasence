#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>

#define AP_SSID "Aquasense"
#define AP_PASSWORD "12345678"

#define WEATHER_API_URL "http://api.openweathermap.org/data/2.5/weather?q=Srivilliputhur&appid=1f12aa13eebc2c8c0d96002e6dde0a3d"
#define LDR_PIN A0

ESP8266WebServer server(80);
DHT dht(0, DHT11);

#define PUMP_PIN 14  
#define SOIL_MOISTURE_PIN 5

WiFiClient wifiClient;
bool weatherIsRainy = false;
bool isManualMode = false;  // Mode toggle variable
bool pumpState = LOW; 

// Function Prototypes
void handleRoot();
void handleGetData();
void handleSetPump();
void handleSetMode();
void fetchWeatherData();

void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(SOIL_MOISTURE_PIN, INPUT);
  pinMode(PUMP_PIN, OUTPUT);

  WiFi.softAP(AP_SSID, AP_PASSWORD);
  IPAddress IP = WiFi.softAPIP();
  Serial.println("Access Point started");
  Serial.print("IP Address: ");
  Serial.println(IP);

  server.on("/", handleRoot);
  server.on("/getData", handleGetData);
  server.on("/setPump", handleSetPump);
  server.on("/setMode", handleSetMode);
  server.begin();

  fetchWeatherData();
}

void handleRoot() {
  String html = R"rawliteral(
    <html>
      <head>
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <link href="https://fonts.googleapis.com/css2?family=Poppins:wght@400;600&display=swap" rel="stylesheet">
        <script>
          setInterval(function() {
            fetch('/getData').then(response => response.json()).then(data => {
              document.getElementById("temperature").textContent = data.temperature;
              document.getElementById("soil").textContent = data.soilIsDry ? "Dry" : "Wet";
              document.getElementById("pump").textContent = data.pumpState ? "ON" : "OFF";
              document.getElementById("weather").textContent = data.weather;
              document.getElementById("ldrWeather").textContent = data.ldrWeather;
              document.getElementById("mode").textContent = data.mode;

              // Update mode button state
              document.getElementById("autoModeBtn").className = data.mode === "Automatic" ? "btn on" : "btn off";
              document.getElementById("manualModeBtn").className = data.mode === "Manual" ? "btn on" : "btn off";

              // Show/hide pump controls based on mode
              if (data.mode === "Manual") {
                document.getElementById("pumpControls").style.display = "flex";
              } else {
                document.getElementById("pumpControls").style.display = "none";
              }

              // Update pump button state
              document.getElementById("pumpOnBtn").className = data.pumpState ? "btn on" : "btn off";
              document.getElementById("pumpOffBtn").className = !data.pumpState ? "btn on" : "btn off";
            });
          }, 1000);
          
          function setPumpState(state) {
            fetch("/setPump?state=" + state);
          }

          function setMode(mode) {
            fetch("/setMode?mode=" + mode);
          }
        </script>
        <style>
          body {
            font-family: 'Poppins', sans-serif;
            background: linear-gradient(135deg, #EAF7FE, #B3E5FC);
            margin: 0;
            padding: 0;
            display: flex;
            flex-direction: column;
            justify-content: space-between;
            min-height: 100vh;
          }
          .top-menu, .bottom-menu {
            background: white;
            padding: 15px;
            box-shadow: 0 4px 10px rgba(0, 0, 0, 0.1);
            text-align: center;
          }
          .top-menu h1 {
            color: #2C3E50;
            font-size: 32px;
            font-weight: bold;
            margin: 0;
          }
          .top-menu .mode-display {
            font-size: 20px;
            color: #555;
            margin-top: 10px;
          }
          .content {
            flex: 1;
            padding: 20px;
            text-align: center;
          }
          .card {
            background: white;
            border-radius: 10px;
            box-shadow: 0 4px 10px rgba(0, 0, 0, 0.1);
            padding: 15px;
            margin: 10px auto;
            max-width: 300px;
          }
          .status {
            font-size: 18px;
            color: #555;
            margin: 10px 0;
          }
          .btn {
            padding: 12px 24px;
            margin: 5px;
            border: none;
            border-radius: 25px;
            cursor: pointer;
            font-size: 16px;
            font-weight: 600;
            transition: all 0.3s ease;
            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
          }
          .btn.on {
            background: linear-gradient(135deg, #4CAF50, #81C784);
            color: white;
          }
          .btn.off {
            background: linear-gradient(135deg, #F44336, #E57373);
            color: white;
          }
          .btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 6px 10px rgba(0, 0, 0, 0.15);
          }
          #pumpControls {
            display: none;
            justify-content: center;
            gap: 10px;
            margin-top: 10px;
          }
        </style>
      </head>
      <body>
        <div class="top-menu">
          <h1>AQUASENSE</h1>
          <div class="mode-display">Current Mode: <span id="mode"></span></div>
          <div>
            <button id="autoModeBtn" class="btn off" onclick="setMode('auto')">Automatic Mode</button>
            <button id="manualModeBtn" class="btn off" onclick="setMode('manual')">Manual Mode</button>
          </div>
        </div>
        <div class="content">
          <div class="card">
            <p class="status"><strong>Temperature:</strong> <span id="temperature"></span></p>
            <p class="status"><strong>Soil Moisture:</strong> <span id="soil"></span></p>
            <p class="status"><strong>Weather:</strong> <span id="weather"></span></p>
            <p class="status"><strong>LDR Weather:</strong> <span id="ldrWeather"></span></p>
            <p class="status"><strong>Pump Status:</strong> <span id="pump"></span></p>
          </div>
        </div>
        <div class="bottom-menu" id="pumpControls">
          <button id="pumpOnBtn" class="btn off" onclick="setPumpState('on')">Turn Pump ON</button>
          <button id="pumpOffBtn" class="btn off" onclick="setPumpState('off')">Turn Pump OFF</button>
        </div>
      </body>
    </html>
  )rawliteral";
  
  server.send(200, "text/html", html);
}

void handleGetData() {
  float t = dht.readTemperature();
  bool soilIsDry = digitalRead(SOIL_MOISTURE_PIN);
  int ldrValue = analogRead(LDR_PIN);

  // Validate temperature reading
  String temperatureDisplay;
  if (isnan(t)) { // Check if the temperature reading is invalid
    temperatureDisplay = "N/A"; // Display "N/A" if the reading is invalid
  } else {
    temperatureDisplay = String(t) + "°C"; // Display the valid temperature
  }

  String weatherStatus = weatherIsRainy ? "Rainy" : "Clear";
  
  String ldrWeather;
  if (ldrValue < 300) { 
    ldrWeather = "Dark";
  } else if (ldrValue < 700) { 
    ldrWeather = "Cloudy";
  } else { 
    ldrWeather = "Bright";
  }

  String jsonResponse = "{";
  jsonResponse += "\"temperature\":\"" + temperatureDisplay + "\","; // Use validated temperature
  jsonResponse += "\"soilIsDry\":" + String(soilIsDry) + ",";
  jsonResponse += "\"pumpState\":" + String(pumpState) + ",";
  jsonResponse += "\"weather\":\"" + weatherStatus + "\",";
  jsonResponse += "\"ldrWeather\":\"" + ldrWeather + "\",";
  jsonResponse += "\"mode\":\"" + String(isManualMode ? "Manual" : "Automatic") + "\"";
  jsonResponse += "}";

  server.send(200, "application/json", jsonResponse);
}

void handleSetPump() {
  if (isManualMode) { // Only allow manual pump control in manual mode
    String state = server.arg("state");
    if (state == "on") {
      pumpState = HIGH;
    } else if (state == "off") {
      pumpState = LOW;
    }
    digitalWrite(PUMP_PIN, pumpState);
    server.send(200, "text/plain", "Pump state set to " + state);
  } else {
    server.send(403, "text/plain", "Cannot control pump in Auto mode");
  }
}

void handleSetMode() {
  String mode = server.arg("mode");
  if (mode == "manual") {
    isManualMode = true;
  } else if (mode == "auto") {
    isManualMode = false;
  }
  server.send(200, "text/plain", "Mode set to " + mode);
}

void fetchWeatherData() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(wifiClient, WEATHER_API_URL);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);
      const char* weather = doc["weather"][0]["main"];
      weatherIsRainy = String(weather) == "Rain";
    }
    http.end();
  }
}

void loop() {
  server.handleClient();
  static unsigned long lastWeatherFetch = 0;
  unsigned long currentMillis = millis();
  
  if (currentMillis - lastWeatherFetch > 60000) { 
    lastWeatherFetch = currentMillis;
    fetchWeatherData();
  }
  
  if (!isManualMode) { // Automatic Mode Controls
    bool soilIsDry = digitalRead(SOIL_MOISTURE_PIN);
    if (soilIsDry && !weatherIsRainy) {
      pumpState = HIGH;
    } else {
      pumpState = LOW;
    }
    digitalWrite(PUMP_PIN, pumpState);
  }
}