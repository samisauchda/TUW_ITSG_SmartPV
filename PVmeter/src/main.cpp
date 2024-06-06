#include <Arduino.h>
#include <WiFi.h>
#include <list>

#include "webServer.h"
#include "Sensor.h"
#include "config.h"
#include <sml/sml_file.h>
#include "smlDebug.h"
#include <esp_LittleFS.h>








#define IR_RECV_D1 3

double energyIn=0.0;
double energyOut=0.0;
double vzTestValue=0.0;
float powerIn=0.0;



TaskHandle_t sensorTaskHandle = NULL;


// Initialize LittleFS
void initLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("An error has occurred while mounting LittleFS");
    return;
  }
  Serial.println("LittleFS mounted successfully");

  listLittleFSFiles();
}





// ##########################################################################################
// void process_message(byte *buffer, size_t len, Sensor *sensor, State sensorState)
// call back function for libSML
// process_message is a wrapper around the parse and publish method of class Sensor
void process_message(byte *buffer, size_t len, Sensor *sensor, State sensorState)
{
  //digitalWrite(LED_BUILTIN, LED_BUILTIN_ON);

  if(sensorState == PROCESS_MESSAGE)
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


  // Initialize LittleFS
  initLittleFS();

  

  // Read credentials from LittleFS
  if (readCredentials()) {
    Serial.println("WiFi credentials available. Starting webServer Task");
    xTaskCreate(
      webServerTask,        // Function to implement the task
      "WebServerTask",      // Name of the task
      10000,                // Stack size in words
      NULL,                 // Task input parameter
      1,                    // Priority of the task
      &webServerTaskHandle); // Task handle
  } else {
    Serial.println("NO WiFi credentials available. Starting credentials Task");
    xTaskCreate(
      credentialsTask,        // Function to implement the task
      "credetialsTask",      // Name of the task
      10000,                // Stack size in words
      NULL,                 // Task input parameter
      1,                    // Priority of the task
      &credentialsTaskHandle); // Task handle
  
  }


  // if no wfi credentials are available: start webserver 1 task to start wifi AP mode and ask for wifi and email readCredentials
  // -> afterwards stop task and start webserver 2 task to connect to wifi and webserver with modules page


  

  
  while (WiFi.status() != WL_CONNECTED)
  {

  }
  // Start the SMTP task
  // // xTaskCreatePinnedToCore(
  // //   smtpTask,             // Function to implement the task
  // //   "SMTPTask",           // Name of the task
  // //   10000,                // Stack size in words
  // //   NULL,                 // Task input parameter
  // //   1,                    // Priority of the task
  // //   &smtpTaskHandle,      // Task handle
  // //   1);                   // Core where the task should run
  // Print saved modules periodically
  // xTaskCreate([](void*) {
  //   while (true) {
  //     printSavedModules();
  //     printEmailParams();
  //     vTaskDelay(10000 / portTICK_PERIOD_MS);  // Print every 10 seconds
  //   }
  // }, "PrintModulesTask", 8192, NULL, 1, NULL);

  xTaskCreate([](void*) {
    

    for(;;) {
    // Allow the task to run indefinitely
      for (std::list<Sensor*>::iterator it = sensors->begin(); it != sensors->end(); ++it)
      {
        (*it)->loop();
      }
      
    }
   }, "SensorTask", 20000, NULL, 1, &sensorTaskHandle);
}

int analogValue = 0;
int test = 0;


void loop() {
  // NOTHING TO DO


}
