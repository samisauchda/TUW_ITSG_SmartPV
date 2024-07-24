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
#include <ESP32Time.h>

#include "csvFunctions.h"



#define IR_RECV_D1 3

double energyIn=0.0;
double energyOut=0.0;
double vzTestValue=0.0;
float powerIn=0.0;

//ESP32Time rtc;
ESP32Time rtc(3600);  // offset in seconds GMT+1
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;
const int   maxClockSyncRetry = 5;
const int   clockRetryDelay = 5000;

std::list<Sensor *> *sensors = new std::list<Sensor *>();
TaskHandle_t sensorTaskHandle = NULL;


typedef struct {
    char datetime[14];  // 13 Zeichen für YYYYMMDD:HHMM und 1 für Nullterminator
    double value1;
} DataRow;




// Initialize LittleFS
void initLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("An error has occurred while mounting LittleFS");
    return;
  }
  Serial.println("LittleFS mounted successfully");

  listLittleFSFiles();
}



void syncRTCtime() {
  int attempt = 0;
  bool success = false;

  struct tm timeinfo;

  while (attempt < maxClockSyncRetry && !success) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    if (getLocalTime(&timeinfo)) {
      rtc.setTimeStruct(timeinfo);
      Serial.print("Time synced with NTP: ");
      Serial.println(rtc.getTime());
      success = true;
    } else {
      printf("Failed to obtain time, retrying %i/%i... \n", attempt, maxClockSyncRetry);
      attempt++;
      delay(clockRetryDelay);
    }
  }

  if (!success) {
    Serial.println("Failed to sync time after maximum retries.");
  }
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

            }
          }

        }
    }

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




void setup() {
  sleep(5);
  // Initialize Serial Monitor
  Serial.begin(115200);

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


  // Create the timer
  timerHandleStopWebserver = xTimerCreate(
      "Task Timer",            // Timer name (for debugging)
      pdMS_TO_TICKS(30000),    // Timer period in ticks (20 seconds)
      pdFALSE,                 // One-shot timer (pdFALSE)
      (void *)0,               // Timer ID (not used)
      timerCallback            // Callback function
  );

  // Start the timer to later stop WebServer
  if (timerHandleStopWebserver != NULL) {
      xTimerStart(timerHandleStopWebserver, 0);
  }
    
  
  // if no wfi credentials are available: start webserver 1 task to start wifi AP mode and ask for wifi and email readCredentials
  // -> afterwards stop task and start webserver 2 task to connect to wifi and webserver with modules page

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

  
  while (WiFi.status() != WL_CONNECTED)
  {

  }

  sleep(5);
  // Configure and sync RTC to NTP
  syncRTCtime();

  // read csv file
  DataRow matrix[MAX_ROWS];
  int num_rows = 200;

  readCSV(csvFilePath);
  //read_csv("/hourly_data.csv", matrix, &num_rows);


  for (int i = 0; i < maxRows; i++) {
    for (int j = 0; j < maxCols; j++) {
      if (data[i][j] != "") {
        Serial.print(data[i][j]);
        if (j < maxCols - 1 && data[i][j+1] != "") {
          Serial.print(", ");
        }
      }
    }
    Serial.println();
  }

  
  //init and start sensor Task, run forever
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
  
}
