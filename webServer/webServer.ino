#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

// Network credentials for Access Point
const char* ssidAP = "ESP32-Access-Point";
const char* passwordAP = "12345678";

// Create an instance of the server
AsyncWebServer server(80);

// Variables to store credentials
String ssid = "";
String password = "";

// File paths to save credentials
const char* ssidPath = "/ssid.txt";
const char* passwordPath = "/password.txt";

// Initialize SPIFFS
void initSPIFFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println("An error has occurred while mounting SPIFFS");
    return;
  }
  Serial.println("SPIFFS mounted successfully");
}

// Read credentials from SPIFFS
void readCredentials() {
  if (SPIFFS.exists(ssidPath)) {
    File file = SPIFFS.open(ssidPath, "r");
    if (file) {
      ssid = file.readStringUntil('\n');
      ssid.trim();
      file.close();
    }
  }

  if (SPIFFS.exists(passwordPath)) {
    File file = SPIFFS.open(passwordPath, "r");
    if (file) {
      password = file.readStringUntil('\n');
      password.trim();
      file.close();
    }
  }
}

// Save credentials to SPIFFS
void saveCredentials(const char* ssid, const char* password) {
  File file = SPIFFS.open(ssidPath, "w");
  if (file) {
    file.println(ssid);
    file.close();
  }

  file = SPIFFS.open(passwordPath, "w");
  if (file) {
    file.println(password);
    file.close();
  }
}

// Connect to Wi-Fi with provided credentials
bool connectToWiFi(const char* ssid, const char* password) {
  Serial.printf("Connecting to %s", ssid);
  WiFi.begin(ssid, password);
  unsigned long startAttemptTime = millis();
  
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    delay(1000);
    Serial.print(".");
  }
  
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to WiFi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("Failed to connect to WiFi");
    return false;
  }
}

void startWebServer(const char* indexPage) {
  server.on("/", HTTP_GET, [indexPage](AsyncWebServerRequest *request) {
    request->send(SPIFFS, indexPage, "text/html");
  });

  // Handle form submission
  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("ssid") && request->hasParam("password")) {
      ssid = request->getParam("ssid")->value();
      password = request->getParam("password")->value();
      saveCredentials(ssid.c_str(), password.c_str());
      request->send(200, "text/html", "Credentials received, attempting to connect...");
      delay(1000);
      WiFi.softAPdisconnect(true);
      if (connectToWiFi(ssid.c_str(), password.c_str())) {
        server.end();
        Serial.println("Showing WebPage");
        startWebServer("/index2.html");
      } else {
        WiFi.softAP(ssidAP, passwordAP);
        Serial.println("Showing credentials page");
        startWebServer("/index.html");
      }
    } else {
      request->send(200, "text/html", "Missing SSID or Password.");
    }
  });

  server.begin();
}

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);

  while(!Serial){}

  // Initialize SPIFFS
  initSPIFFS();

  // Read credentials from SPIFFS
  readCredentials();

  if (ssid.length() > 0 && connectToWiFi(ssid.c_str(), password.c_str())) {
    // If credentials are available and connection is successful, start the server with index2.html
    startWebServer("/index2.html");
  } else {
    // If no credentials or failed connection, start the Access Point and serve index.html
    Serial.print("No credentials available. Starting AP... ");
    WiFi.softAP(ssidAP, passwordAP);
    Serial.println("Access Point started");
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());

    startWebServer("/index.html");
  }
}

void loop() {
  // Nothing needed here for this example
}
