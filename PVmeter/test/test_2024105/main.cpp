#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESP_Mail_Client.h>

#include "webServer.h"
#include "Sensor.h"
#include "config.h"
#include <sml/sml_file.h>
#include "smlDebug.h"
#include <esp_LittleFS.h>
#include "helperFunctions.h"
#include "email.h"

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

String downloadURL;

// Initialize email credentials
EmailCredentials emailCreds;

// Required size for the array
size_t arraySize = 8760; // Number of elements
  

float* PVData = allocateFloatArray(arraySize);
float* SensorMaxPower = allocateFloatArray(arraySize);

struct ergebnisTag {
  float diff_min;
  bool breakdown;
};

struct ergebnisTag ErgebnisWoche[7] = {};

bool comparingStarted = false;


std::list<Sensor *> *sensors = new std::list<Sensor *>();
TaskHandle_t sensorTaskHandle = NULL;

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

void initLittleFS();

struct ergebnisTag CompareOneDay(float Vergleich[], float Mess[], float altersfaktor, int index, int kWp) {
  ergebnisTag erg = {.diff_min = 100, .breakdown = false};
  float diff = 0;
  float max_array2 = 0;

   for (int i = index; i < (index + 24); i++) {
    if (Vergleich[i] != 0) {
      diff = 1 - (Vergleich[i] - (Mess[i] * altersfaktor)) / Vergleich[i]; // bei perioden gibt einen fehler weißt du wieso?
      Serial.println(diff);
      
      if (diff > erg.diff_min && diff > 0) {
        erg.diff_min = diff;
        //Serial.print(" - ");
        //Serial.println(erg.diff_min);
      }
      if (Mess[i] > max_array2){ // für die überprüfung ob überhaupt eine Leistung erzeugt wird
        max_array2 = Mess[i];
      }
    }
  }
  
  
  if(max_array2 == 0 || max_array2 * 0.01 < kWp){ // stellt einen Pointer auf True falls keine/kaum Energie erzeugt wurde
    erg.breakdown = true;
  }
  
  return erg;
}

// Initialize LittleFS
void initLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("An error has occurred while mounting LittleFS");
    return;
  }
  Serial.println("LittleFS mounted successfully");

  listLittleFSFiles();
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
  Serial.print("Free heap: ");
  Serial.println(ESP.getFreeHeap());
  xTaskCreate(
      webServerTask,        // Function to implement the task
      "WebServerTask",      // Name of the task
      16384,                // Stack size in words
      NULL,                 // Task input parameter
      1,                    // Priority of the task
      &webServerTaskHandle); // Task handle
  Serial.print("Free heap: ");
  Serial.println(ESP.getFreeHeap());
  
  while (WiFi.status() != WL_CONNECTED)
  {

  }
  Serial.println("Connected to WiFi. Trying to Sync RTC with NTP Server...");
  sleep(5);
  // Configure and sync RTC to NTP
  syncRTCtime();
  Serial.print("Time is:");
  getFormattedTime();

  
  IPAddress localIP = WiFi.localIP();
  sprintf(ipStr, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]); // Example IP address
  xTaskCreate(sendEmailTaskIPaddress, "SendEmailWithIP", 16384, (void*)ipStr, 1, NULL);


  const char* filename = "/newFile.csv";    // File to save in SPIFFS
  const char* markDataBegin = "time,P";
  const int csvColumn = 1;

  listLittleFSFiles();

  if (!readParametersFromFile(parameterFile)) {
    Serial.println("Failed to read parameters from file");
  } 

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

void loop() {
  delay(10000);

  int Wochentag = rtc.getDayofWeek();
  int Stunde = rtc.getHour(false);
  int Minute = rtc.getMinute();
  float altersfaktor = 1;   // replace

  if(Wochentag = 0 && Stunde==0 && Minute==0 && comparingStarted == false){
    //maybe kill Sensor Task to get ressources for processing

    //if first day of week, and 0:00 -> eval last day of old week, then send Mail
    comparingStarted = true;

    // result always evaluated after day is already finished -> subtract one day
    if(Wochentag == 0) {
      Wochentag = 7;
    } else {
      Wochentag -= 1;
    }

    int indexGestern = calculateDataIndex() -24;
    ErgebnisWoche[Wochentag] = CompareOneDay(PVData, SensorMaxPower, altersfaktor, indexGestern, peakpower);
    //send Mail
    //create Task

  } else if (Stunde==0 && Minute==0 && comparingStarted == false) {
    //maybe kill Sensor Task to get ressources for processing
    // if 0:00 -> eval last day
    comparingStarted = true;

    if(Wochentag == 0) {
      Wochentag = 7;
    } else {
      Wochentag -= 1;
    }

    int indexGestern = calculateDataIndex() -24;
    ErgebnisWoche[Wochentag] = CompareOneDay(PVData, SensorMaxPower, altersfaktor, indexGestern, peakpower);
  } else if (Stunde==0 && Minute==1) {
    comparingStarted == false;
  }

  // Main loop remains empty since tasks are running independently

}