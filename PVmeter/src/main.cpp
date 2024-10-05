#include <Arduino.h>
#include <WiFi.h>
#include <list>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #include "esp_timer.h"

#include "webServer.h"
#include "Sensor.h"
#include "config.h"
#include <sml/sml_file.h>
#include "smlDebug.h"
#include <esp_LittleFS.h>

#include "timeFunctions.h"
#include "csvFunctions.h"



#define IR_RECV_D1 3

// Declare global variables
double lat, lon, peakpower, loss, angle;
int year, aspect, age;
String pvtechchoice, mountingplace;

// Paths for storing the Wi-Fi and email credentials
const char* wifiCredentialsPath = "/wifi.txt";
const char* emailCredentialsPath = "/email.json";
const char* parameterFile = "/params.json";


// Initialize email credentials
EmailCredentials emailCreds;


double energyIn=0.0;
double energyOut=0.0;
double vzTestValue=0.0;
float powerIn=0.0;

std::list<Sensor *> *sensors = new std::list<Sensor *>();
TaskHandle_t sensorTaskHandle = NULL;

// Required size for the array
size_t arraySize = 8760; // Number of elements
  

float* PVData = allocateFloatArray(arraySize);
float* SensorMaxPower = allocateFloatArray(arraySize);




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

    Serial.println("SML FILE DEBUG:");
    DEBUG_SML_FILE(file);

    Serial.println("Get SML message info");

    for (int i = 0; i < file->messages_len; i++)                          // cycle through messages
    {
        sml_message *message = file->messages[i];                         // get messages
        if (*message->message_body->tag == SML_MESSAGE_GET_LIST_RESPONSE) // if message type SML_MESSAGE_GET_LIST_RESPONSE
        {
          sml_list *entry;
          sml_get_list_response *body;
          body = (sml_get_list_response *)message->message_body->data;

          for (entry = body->val_list; entry != NULL; entry = entry->next)
          {
            if (!entry->value)
            { // do not crash on null value
                fprintf(stderr, "Error in data stream. entry->value should not be NULL. Skipping this.\n");
                continue;
            }


            // Leistung 1.0.16.7
            if((entry->obj_name->str[0] == 1) && (entry->obj_name->str[1] == 0) && (entry->obj_name->str[2] == 16) && (entry->obj_name->str[3] == 7)) {
              
              double value = sml_value_to_double(entry->value);
              Serial.println("Entry Value:");
              Serial.println(value);
              int scaler = (entry->scaler) ? *entry->scaler : 0;
              int prec = -scaler;
              if (prec < 0)
                  prec = 0;
              value = value * pow(10, scaler);
              Serial.println("Entry Value scaled:");
              Serial.println(value);

              Serial.print('@');
              Serial.print(rtc.getTime());

              // calculate index for array data access
              int index = calculateDataIndex();
              // save new power value of higher then already saved
              if (SensorMaxPower[index] < value) {
                SensorMaxPower[index] = value;
              }
    
              // ch
              // writeToCSV(value, 99);

            }
          }

        }
    }

    // free the malloc'd memory
    sml_file_free(file);
    }
}




void setup() {
  sleep(5);
  // Initialize Serial Monitor
  Serial.begin(115200);
  esp_log_level_set("*", ESP_LOG_VERBOSE);

  while(!Serial){}


  pinMode(IR_RECV_D1, INPUT_PULLUP);
  pinMode(9, OUTPUT);
  //pinMode(3, INPUT_PULLUP);  // 3 is D1 in ESP32C3


  // configure Sensor
  const SensorConfig *config = SENSOR_CONFIGS;
  for (uint8_t i = 0; i < NUM_OF_SENSORS; i++, config++)
  {
    Sensor *sensor = new Sensor(config, process_message);
    sensors->push_back(sensor);
  }


  // Initialize LittleFS
  initLittleFS();
  listLittleFSFiles();

    
  
  // if no wfi credentials are available: start webserver 1 task to start wifi AP mode and ask for wifi and email readCredentials
  // -> afterwards stop task and start webserver 2 task to connect to wifi and webserver with modules page
  xTaskCreate(
      webServerTask,        // Function to implement the task
      "WebServerTask",      // Name of the task
      10000,                // Stack size in words
      NULL,                 // Task input parameter
      1,                    // Priority of the task
      &webServerTaskHandle); // Task handle
  
  while (WiFi.status() != WL_CONNECTED)
  {

  }
  Serial.println("Connected to WiFi. Trying to Sync RTC with NTP Server...");
  sleep(5);
  // Configure and sync RTC to NTP
  syncRTCtime();
  Serial.println("Time is:");
  getFormattedTime();


  const char* filename = "/newFile.csv";    // File to save in SPIFFS
  const char* markDataBegin = "time,P";
  const int csvColumn = 1;

  listLittleFSFiles();

  if (!readParametersFromFile(parameterFile)) {
    Serial.println("Failed to read parameters from file");
  } else {
    // Print the parameters to confirm they were read successfully
    Serial.println("Parameters loaded successfully:");
    Serial.println("Latitude: " + String(lat));
    Serial.println("Longitude: " + String(lon));
    Serial.println("Year: " + String(year));
    Serial.println("Peak Power: " + String(peakpower));
    Serial.println("Loss: " + String(loss));
    Serial.println("PV Tech Choice: " + pvtechchoice);
    Serial.println("Mounting Place: " + mountingplace);
    Serial.println("Angle: " + String(angle));
    Serial.println("Aspect: " + String(aspect));
    Serial.println("Age: " + String(age));
  }


  // Call the function to read the CSV file
  readCSVtoArray("/newFile.csv", PVData, arraySize, markDataBegin, csvColumn);
  

  //Print CSV data from arrays (for debugging)
  // for (int i = 0; i < arraySize; i++) {
  //   // Serial.print(timeArray[i]);
  //   Serial.print("\t");
  //   Serial.println(PVData[i]);
  // }
  //init and start sensor Task, run forever
  
}

int analogValue = 0;
int test = 0;



void loop() {
  sleep(50);

 
}
