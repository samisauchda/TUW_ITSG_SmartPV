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
#include <ESP_Mail_Client.h>



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

struct ergebnisTag {
  float diff_min;
  bool breakdown;
};

ergebnisTag ergebnisWoche[7];
  
bool auswertungStarted = false;

float* PVData = allocateFloatArray(arraySize);
float* SensorMaxPower = allocateFloatArray(arraySize);


float altersfaktor = 0.8;

int indexTEST = 0;
bool breakdown;
int kWp = 1000;

#define SMTP_HOST "smtp.gmail.com"       // SMTP Server
#define SMTP_PORT 465                    // SMTP port
#define AUTHOR_EMAIL "tuw.itsg.2024@gmail.com" // Sender's email
#define AUTHOR_PASSWORD "rbxpegoflnsownim"  // Sender's email password
#define RECIPIENT_EMAIL "sam.auffenberg@gmail.com" // Recipient email

/* Declare the global used SMTPSession object for SMTP transport */
SMTPSession smtp;

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status);

/* Function to send an email */
void sendEmailTask(void *pvParameters);


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

//funktion die eine Woche auswertet

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

    
  Serial.print("Free heap before WebServerTask: ");
  Serial.println(ESP.getFreeHeap());
  // if no wfi credentials are available: start webserver 1 task to start wifi AP mode and ask for wifi and email readCredentials
  // -> afterwards stop task and start webserver 2 task to connect to wifi and webserver with modules page
  xTaskCreate(
      webServerTask,        // Function to implement the task
      "WebServerTask",      // Name of the task
      4096,                // Stack size in words
      NULL,                 // Task input parameter
      1,                    // Priority of the task
      &webServerTaskHandle); // Task handle
  Serial.print("Free heap after WebServerTask: ");
  Serial.println(ESP.getFreeHeap());
  
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
  // readCSVtoArray("/newFile.csv", PVData, arraySize, markDataBegin, csvColumn);
  // xTaskCreate(
  //     smtpTask,        // Function to implement the task
  //     "smtpTask",      // Name of the task
  //     10000,                // Stack size in words
  //     NULL,                 // Task input parameter
  //     1,                    // Priority of the task
  //     &smtpTaskHandle); // Task handle

  /* Set the network reconnection option */
  MailClient.networkReconnect(true);

  /* Start a task to send an email */
  xTaskCreate(
    sendEmailTask,   // Task function
    "EmailSender",   // Name of task
    8192,            // Stack size (in bytes)
    NULL,            // Task input parameter
    1,               // Priority of the task
    NULL             // Task handle
  );

}


void loop() {
  Serial.print("Free heap: ");
  Serial.println(ESP.getFreeHeap());

  // struct ergebnisTag diff = CompareOneDay(array1, array2, altersfaktor, 0, kWp);
  // Serial.println(diff.diff_min);
  int Wochentag = rtc.getDayofWeek();
  int Stunde = rtc.getHour(true);
  int indexGestern = calculateDataIndex() - 24;
  Serial.println(indexGestern);

  if(Stunde == 0 && Wochentag == 0 && auswertungStarted == false) {
    auswertungStarted == true;
    ergebnisWoche[Wochentag] = CompareOneDay(PVData, SensorMaxPower, 0.8, indexGestern ,kWp);

    //start mail task
  } else if (Stunde == 0 &&  auswertungStarted == false) {
    ergebnisWoche[Wochentag] = CompareOneDay(PVData, SensorMaxPower, 0.8, indexGestern ,kWp);
    // xTaskCreate(
    //   smtpTask,        // Function to implement the task
    //   "smtpTask",      // Name of the task
    //   16384,                // Stack size in words
    //   NULL,                 // Task input parameter
    //   1,                    // Priority of the task
    //   &smtpTaskHandle); // Task handle
  } else if (Stunde == 1){
    auswertungStarted = false;
  }
  // If uhrzeit(00:00) und Montag und auswertung = 0
  //   auswertung = 1
  //   starte auswertung von gestern
  // If uhrzeit(00:00) und auswertung = 0
  //   auswertung = 1
  //   starte auswertung von gestern
  // if uhrzeit(00:01)
  //   auswertung = 0
  // am Ende des Tages oder für den letzten Tag Task starten, die den Tag auswertet
  // einmal die Woche EMail mit Auswertung der Woche
  delay(5000);
}


/* Function to send an email */
void sendEmailTask(void *pvParameters)
{
  /* Set the callback function to get the sending results */
  smtp.callback(smtpCallback);

  /* Declare the Session_Config for user defined session credentials */
  Session_Config config;

  /* Set the session config */
  config.server.host_name = SMTP_HOST;
  config.server.port = SMTP_PORT;
  config.login.email = AUTHOR_EMAIL;
  config.login.password = AUTHOR_PASSWORD;

  config.login.user_domain = F("127.0.0.1");

  /* Time and security config */
  config.time.ntp_server = F("pool.ntp.org,time.nist.gov");
  config.time.gmt_offset = 3;
  config.time.day_light_offset = 0;

  /* Declare the message class */
  SMTP_Message message;

  /* Set the message headers */
  message.sender.name = F("Me (我)");
  message.sender.email = AUTHOR_EMAIL;

  String subject = "Test sending a message (メッセージの送信をテストする)";
  message.subject = subject;

  message.addRecipient(F("Someone (誰か)"), RECIPIENT_EMAIL);

  String textMsg = "This is simple plain text message which contains Chinese and Japanese words.\n";
  textMsg += "这是简单的纯文本消息，包含中文和日文单词\n";
  textMsg += "これは中国語と日本語を含む単純なプレーンテキストメッセージです\n";

  message.text.content = textMsg;
  message.text.transfer_encoding = "base64"; // Encoding for non-ASCII text.
  message.text.charSet = F("utf-8");         // UTF-8 charset for non-ASCII text.
  message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_low;

  /* Set custom message header */
  message.addHeader(F("Message-ID: <abcde.fghij@gmail.com>"));

  /* Connect to the server */
  if (!smtp.connect(&config))
  {
    MailClient.printf("Connection error, Status Code: %d, Error Code: %d, Reason: %s\n", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
    vTaskDelete(NULL); // Delete the task if there's an error
    return;
  }

  /* Check if logged in and authenticated */
  if (!smtp.isLoggedIn())
  {
    Serial.println("Not yet logged in.");
  }
  else
  {
    if (smtp.isAuthenticated())
      Serial.println("Successfully logged in.");
    else
      Serial.println("Connected with no Auth.");
  }

  /* Start sending Email and close the session */
  if (!MailClient.sendMail(&smtp, &message))
    MailClient.printf("Error, Status Code: %d, Error Code: %d, Reason: %s\n", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());

  smtp.sendingResult.clear(); // Clear the result log after sending

  vTaskDelete(NULL); // Delete the task once the email is sent
}

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status)
{
  Serial.println(status.info());

  if (status.success())
  {
    Serial.println("----------------");
    MailClient.printf("Message sent success: %d\n", status.completedCount());
    MailClient.printf("Message sent failed: %d\n", status.failedCount());
    Serial.println("----------------\n");

    for (size_t i = 0; i < smtp.sendingResult.size(); i++)
    {
      SMTP_Result result = smtp.sendingResult.getItem(i);

      MailClient.printf("Message No: %d\n", i + 1);
      MailClient.printf("Status: %s\n", result.completed ? "success" : "failed");
      MailClient.printf("Date/Time: %s\n", MailClient.Time.getDateTimeString(result.timestamp, "%B %d, %Y %H:%M:%S").c_str());
      MailClient.printf("Recipient: %s\n", result.recipients.c_str());
      MailClient.printf("Subject: %s\n", result.subject.c_str());
    }
    Serial.println("----------------\n");

    smtp.sendingResult.clear(); // Clear the results to free up memory
  }
}
