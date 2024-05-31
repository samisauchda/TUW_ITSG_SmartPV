#include <Arduino.h>
#include <WiFi.h>
#include <list>

#include "email.h"
#include "webServer.h"
#include "Sensor.h"
#include "config.h"
#include <sml/sml_file.h>
#include "smlDebug.h"

// Initialize SPIFFS
void initSPIFFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println("An error has occurred while mounting SPIFFS");
    return;
  }
  Serial.println("SPIFFS mounted successfully");
}

#define IR_RECV_D1 3

double energyIn=0.0;
double energyOut=0.0;
double vzTestValue=0.0;
float powerIn=0.0;

// ##########################################################################################
// void process_message(byte *buffer, size_t len, Sensor *sensor, State sensorState)
// call back function for libSML
// process_message is a wrapper around the parse and publish method of class Sensor
//
// 2022-12-07 mh
// - sensor state to support update of dash board
//
// 2022-11-30 mh
// - adapted for http and dashboard update
//
void process_message(byte *buffer, size_t len, Sensor *sensor, State sensorState)
{
  //digitalWrite(LED_BUILTIN, LED_BUILTIN_ON);

  if( sensorState == PROCESS_MESSAGE)
  {
    // Parse
    sml_file *file = sml_file_parse(buffer + 8, len - 16);

    DEBUG_SML_FILE(file);

    // free the malloc'd memory
    sml_file_free(file);

    Serial.print("ms, ");
    Serial.print("P=");
    Serial.print(powerIn);
    Serial.print("W, ");
    Serial.print("E_in=");
    Serial.print(energyIn);
    Serial.print("Wh, ");
    Serial.print("E_out=");
    Serial.print(energyOut);
    Serial.println("Wh");
    Serial.flush();
    }
}

std::list<Sensor *> *sensors = new std::list<Sensor *>();

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);

  while(!Serial){}

  pinMode(IR_RECV_D1, INPUT_PULLUP);
  pinMode(9, OUTPUT);
  //pinMode(3, INPUT_PULLUP);  // 3 is D1 in ESP32C3


  const SensorConfig *config = SENSOR_CONFIGS;
  for (uint8_t i = 0; i < NUM_OF_SENSORS; i++, config++)
  {
    Sensor *sensor = new Sensor(config, process_message);
    sensors->push_back(sensor);
  }


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

  for (std::list<Sensor*>::iterator it = sensors->begin(); it != sensors->end(); ++it)
    {
      (*it)->loop();
    }
}
