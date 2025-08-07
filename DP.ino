#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>

#define AP_SSID "GREENHOUSE_AP"
#define AP_PASSWORD "12345678"

// Define the API URL and LDR pin
#define WEATHER_API_URL "http://api.openweathermap.org/data/2.5/weather?q=Chennai&appid=1f12aa13eebc2c8c0d96002e6dde0a3d"
#define LDR_PIN A0

ESP8266WebServer server(80);
DHT dht(0, DHT11);

#define PUMP_PIN 14  
#define SOIL_MOISTURE_PIN 5

WiFiClient wifiClient;
bool weatherIsRainy = false;
bool manualPumpControl = false; // Variable to track manual pump control
bool pumpState = LOW; // Variable to track pump state

void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(SOIL_MOISTURE_PIN, INPUT);
  pinMode(PUMP_PIN, OUTPUT);

  // Set up the ESP8266 as an access point
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  IPAddress IP = WiFi.softAPIP();
  Serial.println("Access Point started");
  Serial.print("IP Address: ");
  Serial.println(IP);

  server.on("/", handleRoot);
  server.on("/getData", handleGetData);
  server.on("/setPump", handleSetPump);
  server.begin();

  // Fetch weather data (if desired; will not work unless connected to internet)
  fetchWeatherData();
}

void handleRoot() {
  String html = R"(
    <html>
      <head>
        <script>
          setInterval(function() {
            fetch('/getData').then(function(response) {
              return response.json();
            }).then(function(data) {
              document.getElementById('temperature').textContent = data.temperature + '°C';
              document.getElementById('soil').textContent = data.soilIsDry ? 'Dry' : 'Wet';
              document.getElementById('pump').textContent = data.pumpState ? 'ON' : 'OFF';
              document.getElementById('weather').textContent = data.weather;
              document.getElementById('ldrWeather').textContent = data.ldrWeather;
            });
          }, 1000);
          
          function setPumpState(state) {
            fetch('/setPump?state=' + state);
          }
        </script>
        <style>
          body {
            font-family: Arial, sans-serif;
            text-align: center;
            background-color: #EAF7FE;
            background-position: left top, right bottom;
            background-repeat: no-repeat, no-repeat;
          }
          .container {
            display: inline-block;
            background-color: #FFF;
            padding: 20px;
            border-radius: 15px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
            margin-top: 50px;
          }
          .section {
            margin-bottom: 10px;
            border-bottom: 1px solid #E5E5E5;
            padding-bottom: 10px;
          }
          h1 {
            color: darkgreen;
          }
        </style>
      </head>
      <body>
        <div class="container">
          <h1>GREEN HOUSE DATA</h1>
          <div class="section">
            <p><strong>Temperature:</strong> <span id='temperature'></span></p>
            <p><strong>Soil Moisture:</strong> <span id='soil'></span></p>
            <p><strong>Weather:</strong> <span id='weather'></span></p>
            <p><strong>LDR Weather:</strong> <span id='ldrWeather'></span></p>
          </div>
          <div class="section">
            <p><strong>Pump Status:</strong> <span id='pump'></span></p>
            <button onclick=setPumpState('on')>Turn Pump ON</button>
            <button onclick=setPumpState('off')>Turn Pump OFF</button>
          </div>
        </div>
      </body>
    </html>
  )";
  server.send(200, "text/html", html);
}

void handleGetData() {
  float t = dht.readTemperature();
  bool soilIsDry = digitalRead(SOIL_MOISTURE_PIN);
  int ldrValue = analogRead(LDR_PIN);

  String weatherStatus = weatherIsRainy ? "Rainy" : "Clear";
  
  // Adjust LDR weather thresholds based on typical light intensity values
  String ldrWeather;
  if (ldrValue < 300) { // Bright light
    ldrWeather = "Cloudy";
  } else if (ldrValue < 700) { // Moderate light
    ldrWeather = "Bright";
  } else { // Low light
    ldrWeather = "Dark";
  }

  String jsonResponse = "{";
  jsonResponse += "\"temperature\":\"" + String(t) + "\",";
  jsonResponse += "\"soilIsDry\":" + String(soilIsDry) + ",";
  jsonResponse += "\"pumpState\":" + String(pumpState) + ",";
  jsonResponse += "\"weather\":\"" + weatherStatus + "\",";
  jsonResponse += "\"ldrWeather\":\"" + ldrWeather + "\"";
  jsonResponse += "}";

  server.send(200, "application/json", jsonResponse);

  // Automatic pump control based on conditions if not manually controlled
  if (!manualPumpControl) {
    if (t < 20.0 && soilIsDry && !weatherIsRainy) {
      pumpState = HIGH;
    } else {
      pumpState = LOW;
    }
    digitalWrite(PUMP_PIN, pumpState);
  }
}

void handleSetPump() {
  String state = server.arg("state");
  manualPumpControl = true; // Enable manual control
  if (state == "on") {
    pumpState = HIGH;
  } else if (state == "off") {
    pumpState = LOW;
  }
  digitalWrite(PUMP_PIN, pumpState);
  server.send(200, "text/plain", "Pump state set to " + state);
}

void fetchWeatherData() {
  // Fetching weather data will only work if the device is connected to the internet.
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
  
  if (currentMillis - lastWeatherFetch > 60000) { // Fetch weather data every minute
    lastWeatherFetch = currentMillis;
    fetchWeatherData();
  }
  
  // Reset manual control if conditions are met
  if (manualPumpControl) {
    bool soilIsDry = digitalRead(SOIL_MOISTURE_PIN);
    if (soilIsDry && !weatherIsRainy) {
      pumpState = HIGH; // Keep the pump on if soil is dry and weather is not rainy
    } else {
      pumpState = LOW;  // Turn off the pump otherwise
    }
    digitalWrite(PUMP_PIN, pumpState);
  }
}
