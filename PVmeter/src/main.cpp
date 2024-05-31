#include <Arduino.h>
#include <WiFi.h>

#include "email.h"
#include "webServer.h"

// Initialize SPIFFS
void initSPIFFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println("An error has occurred while mounting SPIFFS");
    return;
  }
  Serial.println("SPIFFS mounted successfully");
}

#define IR_RECV_D1 3


void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);

  while(!Serial){}

  pinMode(IR_RECV_D1, INPUT_PULLUP);
  pinMode(9, OUTPUT);
  //pinMode(3, INPUT_PULLUP);  // 3 is D1 in ESP32C3

  // Initialize SPIFFS
  initSPIFFS();

  // Read credentials from SPIFFS
  readCredentials();

  // init EMail Server
  initEmail();

  if (ssid.length() > 0 && connectToWiFi(ssid.c_str(), password.c_str())) {
    // If credentials are available and connection is successful, start the server with index2.html
    startWebServer("/index2.html", 1);
  } else {
    // If no credentials or failed connection, start the Access Point and serve index.html
    Serial.print("No credentials available. Starting AP... ");
    WiFi.softAP(ssidAP, passwordAP);
    Serial.println("Access Point started");
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());

    startWebServer("/index.html", 0);
  }

  // Print saved modules periodically
  xTaskCreate([](void*) {
    while (true) {
      printSavedModules();
      printEmailParams();
      vTaskDelay(10000 / portTICK_PERIOD_MS);  // Print every 10 seconds
    }
  }, "PrintModulesTask", 8192, NULL, 1, NULL);
}

int analogValue = 0;
int test = 0;


void loop() {
  // NOTHING TO DO

  if(digitalRead(IR_RECV_D1)){
    Serial.println("D1 high");
  } else {
    Serial.println("D1 low");
  }
  delay(200);
}
