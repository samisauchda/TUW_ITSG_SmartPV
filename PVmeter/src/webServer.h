#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>

#include "helperFunctions.h"

// Network credentials for Access Point
const char* ssidAP = "ESP32-Access-Point";
const char* passwordAP = "12345678";

// Variables to store credentials
String ssid = "";
String password = "";

// File paths to save credentials
const char* ssidPath = "/ssid.txt";
const char* passwordPath = "/password.txt";

// Create an instance of the server
AsyncWebServer server(80);

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

void startWebServer(const char* indexPage, const bool connected) {
  server.on("/", HTTP_GET, [indexPage](AsyncWebServerRequest *request) {
    request->send(SPIFFS, indexPage, "text/html");
  });

  if (connected) {
    // handles for webserver when conected to home wifi
    

    server.on("/add", HTTP_POST, [](AsyncWebServerRequest *request) {
      JsonDocument doc;
      if (SPIFFS.exists("/modules.json")) {
        File file = SPIFFS.open("/modules.json", FILE_READ);
        deserializeJson(doc, file);
        file.close();
      }

      JsonArray modules = doc["modules"].as<JsonArray>();
      if (modules.isNull()) {
        modules = doc.createNestedArray("modules");
      }

      for (int i = 1; ; i++) {
        String nameParam = "name" + String(i);
        String maxPowerParam = "maxPower" + String(i);
        String angleParam = "angle" + String(i);

        if (!request->hasParam(nameParam, true)) {
          break;
        }

        JsonObject module = modules.createNestedObject();
        module["name"] = request->getParam(nameParam, true)->value();
        module["maxPower"] = request->getParam(maxPowerParam, true)->value().toFloat();
        module["angle"] = request->getParam(angleParam, true)->value().toInt();
      }

      File file = SPIFFS.open("/modules.json", FILE_WRITE);
      serializeJson(doc, file);
      file.close();

      request->send(200, "text/plain", "Module data saved");
    });

    // Route to handle Email Parameters
    server.on("/addEmailParams", HTTP_POST, [](AsyncWebServerRequest *request) {
      
      String serverParam = "SMTPserver";
      String userParam = "SMTPuser";
      String passParam = "SMTPpass";
      String tlsPortParam = "SMTPportTLS";
      String sslPortParam = "SMTPportSSL";
      String TLSSSLParam = "TLSSSL";
      String receiverMailParam = "receiverMAIL";

      //JsonObject email = emails.createNestedObject();
      JsonDocument email;
      email["SMTPServer"] = request->getParam(serverParam, true)->value();
      email["SMTPUser"] = request->getParam(userParam, true)->value();
      email["SMTPPass"] = request->getParam(passParam, true)->value();
      email["TLSPort"] = request->getParam(tlsPortParam, true)->value().toInt();
      email["SSLPort"] = request->getParam(sslPortParam, true)->value().toInt();
      email["TLSSSL"] = request->getParam(TLSSSLParam, true)->value();
      email["receiverMail"] = request->getParam(receiverMailParam, true)->value();

      SPIFFS.remove("/email.json");
      
      File file = SPIFFS.open("/email.json", FILE_WRITE);
      if (!file) {
        Serial.println("Failed to open file for writing");
        return;
      }
      serializeJson(email, file);
      file.close();
      Serial.println("Parameters saved to /params.json");
      request->send(200, "text/plain", "Email params saved");

      printEmailParams();
    });

    // Route to get the saved modules
    server.on("/modules", HTTP_GET, [](AsyncWebServerRequest *request) {
      File file = SPIFFS.open("/modules.json", FILE_READ);
      String json;
      if (file) {
        json = file.readString();
        file.close();
      } else {
        json = "{\"modules\":[]}";
      }
      request->send(200, "application/json", json);
    });
    // Route to delete a module
    server.on("/delete", HTTP_GET, [](AsyncWebServerRequest *request) {
      if (request->hasParam("index")) {
        int index = request->getParam("index")->value().toInt();
        JsonDocument doc;
        if (SPIFFS.exists("/modules.json")) {
          File file = SPIFFS.open("/modules.json", FILE_READ);
          deserializeJson(doc, file);
          file.close();
        }

        JsonArray modules = doc["modules"].as<JsonArray>();
        if (!modules.isNull() && index >= 0 && index < modules.size()) {
          modules.remove(index);

          File file = SPIFFS.open("/modules.json", FILE_WRITE);
          serializeJson(doc, file);
          file.close();

          request->send(200, "text/plain", "Module deleted");
        } else {
          request->send(400, "text/plain", "Invalid module index");
        }
      } else {
        request->send(400, "text/plain", "Index parameter missing");
      }
    });

    server.on("/deleteAll", HTTP_POST, [](AsyncWebServerRequest *request){
      bool dataDeleted = SPIFFS.remove("/modules.json");
      bool wifiSSIDDeleted = SPIFFS.remove("/ssid.txt");
      bool wifiPASSDeleted = SPIFFS.remove("/password.txt");
  
      if (dataDeleted && wifiSSIDDeleted && wifiPASSDeleted) {
        request->send(200, "text/plain", "Data and WiFi credentials deleted successfully");
      } else {
        request->send(500, "text/plain", "Failed to delete data and/or WiFi credentials");
      }
    });
  } else {
    // handles for credential submission on AP mode
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
          startWebServer("/index2.html", 1);
        } else {
          WiFi.softAP(ssidAP, passwordAP);
          Serial.println("Showing credentials page");
          startWebServer("/index.html", 0);
        }
      } else {
        request->send(200, "text/html", "Missing SSID or Password.");
      }
    });
  }
  
  server.begin();
  Serial.println("HTTP server started");
}