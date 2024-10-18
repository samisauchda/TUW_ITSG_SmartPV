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

#include "timeFunctions.h"
#include "csvFunctions.h"
#include "email.h"

#define IR_RECV_D1 3

// Declare global variables
float lat, lon, peakpower, loss, angle, degradation20Jahre;
int year, aspect, age;
String pvtechchoice, mountingplace;

// Paths for storing the Wi-Fi and email credentials
const char* wifiCredentialsPath = "/wifi.txt";
const char* emailCredentialsPath = "/email.json";
const char* parameterFile = "/params.json";

String downloadURL;

const char* filename = "/PV_GIS_Data.csv";    // File to save in SPIFFS
const char* markDataBegin = "time,P";
const int csvColumn = 1;

// Initialize email credentials
EmailCredentials emailCreds;

// Required size for the array
size_t arraySize = 8784; // Number of elements
  

float* PVData = allocateFloatArray(arraySize);
float* SensorMaxPower = allocateFloatArray(arraySize);


std::list<Sensor *> *sensors = new std::list<Sensor *>();
TaskHandle_t sensorTaskHandle = NULL;

struct ergebnisTag ErgebnisWoche[7] = {};

bool comparingStarted = false;

void initLittleFS();

float get_Altersfaktor(float given_faktor = 0.8, int alter = 0){
  float faktor = (1 - given_faktor)/20;
  float altersfaktor = 1 - (alter * faktor);
  return altersfaktor;
}

bool check_breakdown(struct ergebnisTag ErgebnisWoche[7]){
  bool breakdown_check = false;
  for( int i = 6; i >= 0; i--){
    if (ErgebnisWoche[i].breakdown == true){
      breakdown_check= true;
    }
    else {
      break;
    }
  }
  return breakdown_check;
}

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
            }
          }

        }
    }

    // free the malloc'd memory
    sml_file_free(file);
    }
}



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

    
  
  // if no wfi credentials are available: start webserver 1 task to start wifi AP mode and ask for wifi and email readCredentials
  // -> afterwards stop task and start webserver 2 task to connect to wifi and webserver with modules page
  Serial.print("Free heap: ");
  Serial.println(ESP.getFreeHeap());
  xTaskCreate(
      webServerTask,        // Function to implement the task
      "WebServerTask",      // Name of the task
      8192,                // Stack size in words
      NULL,                 // Task input parameter
      1,                    // Priority of the task
      &webServerTaskHandle); // Task handle
  Serial.print("Free heap: ");
  Serial.println(ESP.getFreeHeap());
  
  while (WiFi.status() != WL_CONNECTED)
  {

  }





  // csv einlesen
  readCSVtoArray(filename, PVData,  arraySize, markDataBegin, csvColumn);
  readCSVtoArray("/SensorData.csv", SensorMaxPower,  arraySize, "", 0);

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
   }, "SensorTask", 8192, NULL, 1, &sensorTaskHandle);  

}

void loop() {
  delay(10000);

  int Wochentag = rtc.getDayofWeek();
  int Stunde = rtc.getHour(false);
  int Minute = rtc.getMinute();

  Serial.println(String(Minute));

  if(Wochentag = 0 && Stunde==0 && Minute==0 && comparingStarted == false){
  // if((Minute==0 % 2 == 0) && comparingStarted == false){
    //maybe kill Sensor Task to get ressources for processing


    //if first day of week, and 0:00 -> eval last day of old week, then send Mail
    comparingStarted = true;

    // result always evaluated after day is already finished -> subtract one day
    if(Wochentag == 0) {
      Wochentag = 7;
    } else {
      Wochentag -= 1;
    }

    float altersfaktor = get_Altersfaktor(degradation20Jahre, age);
    int indexGestern = calculateDataIndex() -24;
    ErgebnisWoche[Wochentag] = CompareOneDay(PVData, SensorMaxPower, altersfaktor, indexGestern, peakpower);
    saveArrayToCSV("/SensorData.csv", SensorMaxPower, arraySize);
    //send Mail
    // Create the task, passing the parameters as a void pointer
    xTaskCreate(sendEmailTaskWeekly, "EmailTask", 8192, NULL, 1, NULL);  

  } else if (Stunde==0 && Minute==0 && comparingStarted == false) {
        //maybe kill Sensor Task to get ressources for processing
    // if 0:00 -> eval last day
    comparingStarted = true;

    if(Wochentag == 0) {
      Wochentag = 7;
    } else {
      Wochentag -= 1;
    }
    float altersfaktor = get_Altersfaktor(degradation20Jahre, age);
    int indexGestern = calculateDataIndex() -24;
    ErgebnisWoche[Wochentag] = CompareOneDay(PVData, SensorMaxPower, altersfaktor, indexGestern, peakpower);
    saveArrayToCSV("/SensorData.csv", SensorMaxPower, arraySize);

    //remove later!!!
    xTaskCreate(sendEmailTaskWeekly, "EmailTask", 8192, NULL, 1, NULL);  

  } else if (Stunde==0 && Minute==1 && comparingStarted == true) {
  // } else if((Minute+1==0 % 2 == 0) && comparingStarted == true){
    comparingStarted == false;
  }

  // Main loop remains empty since tasks are running independently

}