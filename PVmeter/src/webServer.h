#ifndef WEBSERVER_H_
#define WEBSERVER_H_

#include <string.h> // Include for string handling


#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
//#include <SPIFFS.h>
#include <esp_LittleFS.h>
#include <ESP_Mail_Client.h>
// #include <ESP32_MailClient.h>
#include <HTTPClient.h>
#include "helperFunctions.h"
#include "csvFunctions.h"
#include "Sensor.h"
#include "email.h"
#include "timeFunctions.h"
// #include "download.h"

// Network credentials for Access Point
const char* ssidAP = "ESP32-Access-Point";
const char* passwordAP = "12345678";

// Variables to store credentials
String ssid = "";
String password = "";

extern size_t arraySize;
extern const char* filename;    // File to save in SPIFFS
extern const char* markDataBegin;
extern const int csvColumn;

// File to store parameters
extern const char* parameterFile;

// Declare the variables as extern so that they can be used here
extern float lat, lon, peakpower, loss, angle, degradation20Jahre;
extern int year, aspect, age;
extern String pvtechchoice, mountingplace;

extern TaskHandle_t sensorTaskHandle;
extern std::list<Sensor *> *sensors;

// Create an instance of the server
AsyncWebServer credentialsServer(80);
AsyncWebServer server(80);
// DNSServer dnsServer;

// IP-Adresse des Captive Portals
IPAddress apIP(192, 168, 4, 1);


TaskHandle_t webServerTaskHandle = NULL;
TaskHandle_t credentialsTaskHandle = NULL;
TaskHandle_t smtpTaskHandle = NULL;
TaskHandle_t downloadFileHandle = NULL;

TimerHandle_t timerHandleStopWebserver = NULL;


// Paths for storing the Wi-Fi and email credentials
extern const char* wifiCredentialsPath;
extern const char* emailCredentialsPath;
extern String downloadURL;

// Initialize email credentials
extern EmailCredentials emailCreds;


typedef struct {
    String message_to_send;
} TaskParams;

void webServerTask(void * parameter);
void credentialsTask(void * parameter);
void smtpTask(void * parameter);

bool connectToWiFi();

String readParameters();
void saveParameters(AsyncWebServerRequest *request);
void deleteParameters();
String readHTMLFile(const char * filePath);
void handleFileDownload(AsyncWebServerRequest *request);

// Forward declare the function to read parameters
bool readParametersFromFile(const char* path);


extern float* PVData;

void saveWiFiCredentials(String ssid, String password);
void saveEmailCredentials(EmailCredentials creds);
void deleteWiFiCredentials();
void deleteEmailCredentials();
String getSavedWiFiCredentials();
String getSavedEmailCredentials(); 

void startWebServer(const char* htmlPage);
void webServerTask(void *parameter);

void startDownloadTask(const String &filepath, const String &url);
void downloadFileToLittleFS(const String &filepath, const String &url);


// Connect to Wi-Fi with provided credentials
bool connectToWiFi() {
  // Optionally, stop any running access point mode
  WiFi.softAPdisconnect(true); // Shut down the AP mode, if an

  // First, disconnect from any previous Wi-Fi connections and stop any AP modes
  WiFi.disconnect(true);  // Disconnect from current Wi-Fi network
  WiFi.mode(WIFI_OFF);     // Turn off Wi-Fi to reset all states
  delay(1000);             // Brief delay to ensure the Wi-Fi mode is stopped

  Serial.printf("Reading WiFi credentials from %s \n", wifiCredentialsPath);
  File wifiFile = LittleFS.open(wifiCredentialsPath, "r");
  if (!wifiFile) {
    Serial.println("No Wi-Fi credentials found!");
    return false;
  }
  ssid = wifiFile.readStringUntil('\n');
  ssid.trim();

  password = wifiFile.readStringUntil('\n');
  password.trim();

  wifiFile.close();

  Serial.printf("Connecting to %s...", ssid);
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
    char ipAddress[16]; // Adjust size if necessary
    IPAddress localIP = WiFi.localIP();
    snprintf(ipAddress, sizeof(ipAddress), "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);
    xTaskCreate(sendEmailTaskIPaddress, "SendEmailWithIP", 32768, (void*)ipAddress, 1, NULL);

    Serial.println("Connected to WiFi. Trying to Sync RTC with NTP Server...");
    sleep(5);
    // Configure and sync RTC to NTP
    syncRTCtime();

    return true;
  } else {
    Serial.println("Failed to connect to WiFi");
    return false;
  }
}

void timerCallback(TimerHandle_t xTimer) {
    Serial.println("Timer expired, killing webserver task...");
    if (webServerTaskHandle != NULL) {
        vTaskDelete(webServerTaskHandle);
        webServerTaskHandle = NULL; // Clean up the handle

        Serial.println("Starting Sensor Task...");
        //starting Sensor Task
        xTaskCreate([](void*) {
          for(;;) {
          // Allow the task to run indefinitely
            for (std::list<Sensor*>::iterator it = sensors->begin(); it != sensors->end(); ++it)
            {
              (*it)->loop();
            }

            UBaseType_t remainingStack = uxTaskGetStackHighWaterMark(NULL); // NULL gets current task
            Serial.print("Sensor Remaining stack: ");
            Serial.println(remainingStack);
            
          }
        }, "SensorTask", 2048, NULL, 1, &sensorTaskHandle);
        return;
    }
    Serial.println("WebServer task not started or already killed. Doing nothing...");
}

void startCredentialsServer(const char* htmlPage){

  credentialsServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String savedWifi = getSavedWiFiCredentials();
    String savedEmail = getSavedEmailCredentials();
    String htmlContent = readHTMLFile("/index.html");
    htmlContent.replace("%SAVEDWIFI%", savedWifi);
    htmlContent.replace("%SAVEDEMAIL%", savedEmail);
    request->send(200, "text/html", htmlContent);
  });

  credentialsServer.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/style.css", "text/css");
  });

  credentialsServer.on("/saveWifi", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
      ssid = request->getParam("ssid", true)->value();
      password = request->getParam("password", true)->value();
      saveWiFiCredentials(ssid, password);
      request->redirect("/");
    }
  });

  // Save email credentials
  credentialsServer.on("/saveEmail", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("smtpServer", true) && request->hasParam("smtpUser", true)
        && request->hasParam("smtpPass", true) && request->hasParam("receiverMail", true)) {
      emailCreds.smtpServer = request->getParam("smtpServer", true)->value();
      emailCreds.smtpUser = request->getParam("smtpUser", true)->value();
      emailCreds.smtpPass = request->getParam("smtpPass", true)->value();
      emailCreds.smtpPortTLS = request->getParam("smtpPortTLS", true)->value().toInt();
      emailCreds.smtpPortSSL = request->getParam("smtpPortSSL", true)->value().toInt();
      emailCreds.smtpSecurity = request->getParam("smtpSecurity", true)->value();
      emailCreds.receiverMail = request->getParam("receiverMail", true)->value();
      saveEmailCredentials(emailCreds);
      request->redirect("/");
    }
  });

  // Delete Wi-Fi credentials
  credentialsServer.on("/deleteWifi", HTTP_GET, [](AsyncWebServerRequest *request){
    deleteWiFiCredentials();
    request->redirect("/");
  });

  // Delete email credentials
  credentialsServer.on("/deleteEmail", HTTP_GET, [](AsyncWebServerRequest *request){
    deleteEmailCredentials();
    request->redirect("/");
  });

  // Connect to Wi-Fi with stored credentials
  credentialsServer.on("/connectWifi", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", "Credentials received, attempting to connect...");
    credentialsServer.end();
    // dnsServer.stop();
    Serial.println("deleting credentials Task...");
    
    Serial.println("deleted credentials Task. Starting WebServer task");
    xTaskCreate(
      webServerTask,        // Function to implement the task
      "WebServerTask",      // Name of the task
      8192,                // Stack size in words
      NULL,                 // Task input parameter
      1,                    // Priority of the task
      &webServerTaskHandle); // Task handle
    vTaskDelete(credentialsTaskHandle);
    Serial.println("WebServer Task Started.");
  });

  credentialsServer.begin();
  Serial.println("HTTP server started");
}

void webServerTask(void * parameter) {
  
  if (connectToWiFi()) {
    // If credentials are available and connection is successful, start the server with index2.html
    startWebServer("/index2.html");
    delay(1000);
    

  } else {
    Serial.println("Wrong credentials or WiFi not found");
    Serial.print("Free heap: ");
    Serial.println(ESP.getFreeHeap());

    xTaskCreate(
        credentialsTask,        // Function to implement the task
        "CredentialsTask",      // Name of the task
        8192,                // Stack size in words
        NULL,                 // Task input parameter
        1,                    // Priority of the task
        &credentialsTaskHandle); // Task handle
    Serial.println("credentials Task Started.");
    vTaskDelete(webServerTaskHandle);
    Serial.println("deleted WebServer Task. Starting Credentials task");
    Serial.print("Free heap: ");
    Serial.println(ESP.getFreeHeap());
  }

  //vTaskDelete(webServerTaskHandle);
  for(;;) {
    // Allow the task to run indefinitely
    // delay(10000);
    // UBaseType_t remainingStack = uxTaskGetStackHighWaterMark(NULL); // NULL gets current task
    // Serial.print("WebServer Remaining stack: ");
    // Serial.println(remainingStack);
    //Serial.println(WiFi.status());
  }
}

void credentialsTask(void * parameter){
  Serial.println("Credentials Task started");
  // If no credentials or failed connection, start the Access Point and serve index.html
  Serial.print("No credentials available. Starting AP... ");
  WiFi.softAP(ssidAP, passwordAP);
  Serial.println("Access Point started");
  Serial.print("IP Address: ");
  Serial.println(apIP);
    // Setze die IP-Adresse des ESP32 im Access Point-Modus
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

  startCredentialsServer("/index.html");

  for(;;) {
    delay(1000);
    UBaseType_t remainingStack = uxTaskGetStackHighWaterMark(NULL); // NULL gets current task
    Serial.print("CredentialsTask Remaining stack: ");
    Serial.println(remainingStack);
  }
}

void startWebServer(const char* htmlPage) {
  // Serve HTML form
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String savedParams = readParameters();
    String savedWifi = getSavedWiFiCredentials();
    String savedEmail = getSavedEmailCredentials();
    String htmlContent = readHTMLFile("/index2.html");
    htmlContent.replace("%SAVEDPARAMS%", savedParams);
    htmlContent.replace("%SAVEDWIFI%", savedWifi);
    htmlContent.replace("%SAVEDEMAIL%", savedEmail);
    request->send(200, "text/html", htmlContent);
  });

   // Route to load style.css file
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/style.css", "text/css");
  });

  // Handle form submission and save parameters
  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("Saving parameters");
    saveParameters(request);
    request->redirect("/");
    readParametersFromFile(parameterFile);
  });

  // Handle delete button
  server.on("/delete", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("deleting all parameters");
    deleteParameters();
    request->redirect("/");
  });

  server.on("/downloadFile", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("Calling downloadFile function");

        // xTaskCreate(
        //   downloadTask,     // Task function
        //   "DownloadTask",   // Name of the task
        //   8192,             // Stack size (in bytes)
        //   NULL,             // No parameters are passed to the task
        //   1,                // Priority of the task
        //   NULL              // Task handle
        // );
        // String downloadURL = buildURL("seriescalc");
        // Serial.println(downloadURL);
        
        // Start the download task
        //startDownloadTask("/hourly_data.csv", downloadURL);

        // Respond immediately to the client
        request->send(200, "text/plain", "Download started.");
    });

  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {},
    [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
      if (!index) {
        Serial.printf("UploadStart: %s\n", filename.c_str());
        if (LittleFS.exists("/PV_GIS_Data.csv")) {
          LittleFS.remove("/PV_GIS_Data.csv");
          Serial.printf("Deleted existing file.");
        }
        listLittleFSFiles();
        // Open file for writing (create if it doesn't exist)
        File file = LittleFS.open("/PV_GIS_Data.csv", FILE_WRITE);
        if (!file) {
          Serial.println("Failed to open file for writing");
          return;
        }
        file.close();
      }

      // Open file in append mode and write to it
      File file = LittleFS.open("/PV_GIS_Data.csv", FILE_APPEND);
      if (file) {
        if (file.write(data, len) != len) {
          Serial.println("Failed to write file");
        }
        file.close();
      }

      if (final) {
        Serial.printf("UploadEnd: %s, %u B\n", filename.c_str(), index + len);
        request->send(200, "text/plain", "File Uploaded Successfully!");
        readCSVtoArray("/PV_GIS_Data.csv", PVData,  arraySize, markDataBegin, csvColumn);
      }
  });


  server.on("/sendTestMail", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("Trying to send Test mail");
    Serial.print("Free heap: ");
    Serial.println(ESP.getFreeHeap());
    xTaskCreate(sendEmailTaskIPaddress, "Send Email Task", 8192, NULL, 1, NULL);
    // sendEmail();
    request->redirect("/");
  });

    // Delete Wi-Fi credentials
  server.on("/deleteWifi", HTTP_GET, [](AsyncWebServerRequest *request){
    deleteWiFiCredentials();
    request->redirect("/");
  });

  server.on("/saveEmail", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("smtpServer", true) && request->hasParam("smtpUser", true)
        && request->hasParam("smtpPass", true) && request->hasParam("receiverMail", true)) {
      emailCreds.smtpServer = request->getParam("smtpServer", true)->value();
      emailCreds.smtpUser = request->getParam("smtpUser", true)->value();
      emailCreds.smtpPass = request->getParam("smtpPass", true)->value();
      emailCreds.smtpPortTLS = request->getParam("smtpPortTLS", true)->value().toInt();
      emailCreds.smtpPortSSL = request->getParam("smtpPortSSL", true)->value().toInt();
      emailCreds.smtpSecurity = request->getParam("smtpSecurity", true)->value();
      emailCreds.receiverMail = request->getParam("receiverMail", true)->value();
      saveEmailCredentials(emailCreds);
      request->send(200, "text/plain", "Email credentials saved successfully");
    }
  });

  // Delete email credentials
  server.on("/deleteEmail", HTTP_GET, [](AsyncWebServerRequest *request){
    deleteEmailCredentials();
    request->redirect("/");
  });

  server.on("/download", HTTP_GET, handleFileDownload);
  

  server.begin();
  Serial.println("HTTP server started");
}



// Function to read parameters from JSON file
String readParameters() {
  if (!LittleFS.exists(parameterFile)) {
    return "No parameters saved yet.";
  }

  File file = LittleFS.open(parameterFile, "r");
  if (!file) {
    return "Failed to read the file.";
  }

  // Parse the JSON file
  StaticJsonDocument<1024> jsonDoc;
  DeserializationError error = deserializeJson(jsonDoc, file);
  file.close();

  if (error) {
    return "Failed to parse the file.";
  }

  // Convert JSON to readable format
  String params;
  params += "Latitude: " + String((const char*)jsonDoc["lat"]) + "<br>";
  params += "Longitude: " + String((const char*)jsonDoc["lon"]) + "<br>";
  params += "Year: " + String((const char*)jsonDoc["year"]) + "<br>";
  params += "Peak Power: " + String((const char*)jsonDoc["peakpower"]) + "<br>";
  params += "Loss: " + String((const char*)jsonDoc["loss"]) + "<br>";
  params += "PV Tech Choice: " + String((const char*)jsonDoc["pvtechchoice"]) + "<br>";
  params += "Mounting Place: " + String((const char*)jsonDoc["mountingplace"]) + "<br>";
  params += "Fixed: " + String((const char*)jsonDoc["fixed"]) + "<br>";
  params += "Angle: " + String((const char*)jsonDoc["angle"]) + "<br>";
  params += "Aspect: " + String((const char*)jsonDoc["aspect"]) + "<br>";
  params += "Age: " + String((const char*)jsonDoc["age"]) + "<br>";
  params += "Degradation: " + String((const char*)jsonDoc["degradation"]) + "<br>";
  
  return params;
}

// Function to save parameters to JSON file
void saveParameters(AsyncWebServerRequest *request) {
  // Create a JSON document to hold the parameters
  StaticJsonDocument<1024> jsonDoc;

  if (request->hasParam("lat")) jsonDoc["lat"] = request->getParam("lat")->value();
  if (request->hasParam("lon")) jsonDoc["lon"] = request->getParam("lon")->value();
  if (request->hasParam("year")) jsonDoc["year"] = request->getParam("year")->value();
  if (request->hasParam("peakpower")) jsonDoc["peakpower"] = request->getParam("peakpower")->value();
  if (request->hasParam("loss")) jsonDoc["loss"] = request->getParam("loss")->value();
  if (request->hasParam("pvtechchoice")) jsonDoc["pvtechchoice"] = request->getParam("pvtechchoice")->value();
  if (request->hasParam("mountingplace")) jsonDoc["mountingplace"] = request->getParam("mountingplace")->value();
  if (request->hasParam("fixed")) jsonDoc["fixed"] = request->getParam("fixed")->value();
  if (request->hasParam("angle")) jsonDoc["angle"] = request->getParam("angle")->value();
  if (request->hasParam("aspect")) jsonDoc["aspect"] = request->getParam("aspect")->value();
  if (request->hasParam("age")) jsonDoc["age"] = request->getParam("age")->value();
  if (request->hasParam("degradation")) jsonDoc["degradation"] = request->getParam("degradation")->value();

  // Save JSON to file
  File file = LittleFS.open(parameterFile, "w");
  if (file) {
    serializeJson(jsonDoc, file);
    file.close();
    Serial.println("Saving JSON with parameters");
  } else {
    Serial.println("Failed to open the file for writing.");
  }
}

// Function to delete parameters (delete the JSON file)
void deleteParameters() {
  LittleFS.remove(parameterFile);
}

String readHTMLFile(const char * filePath) {
  String htmlContent;
  File file = LittleFS.open(filePath, "r");
  if (file) {
    while (file.available()) {
      htmlContent += file.readString();
    }
    file.close();
  }
  return htmlContent;
}

// Save Wi-Fi credentials to a text file
void saveWiFiCredentials(String ssid, String password) {
  File wifiFile = LittleFS.open(wifiCredentialsPath, "w");
  if (!wifiFile) {
    Serial.println("Failed to open Wi-Fi credentials file for writing");
    return;
  }

  wifiFile.println(ssid);
  wifiFile.println(password);
  wifiFile.close();
  Serial.printf("Saved WifiCredentials. SSID: %s, Password: %s", ssid, password);
}

// Save email credentials
void saveEmailCredentials(EmailCredentials creds) {
  File emailFile = LittleFS.open(emailCredentialsPath, "w");
  if (!emailFile) {
    Serial.println("Failed to open email credentials file for writing");
    return;
  }

  emailCreds = creds;

  StaticJsonDocument<256> doc;
  doc["smtpServer"] = creds.smtpServer;
  doc["smtpUser"] = creds.smtpUser;
  doc["smtpPass"] = creds.smtpPass;
  doc["smtpPortTLS"] = creds.smtpPortTLS;
  doc["smtpPortSSL"] = creds.smtpPortSSL;
  doc["smtpSecurity"] = creds.smtpSecurity;
  doc["receiverMail"] = creds.receiverMail;

  serializeJson(doc, emailFile);
  emailFile.close();
}

// Delete Wi-Fi credentials
void deleteWiFiCredentials() {
  LittleFS.remove(wifiCredentialsPath);
}

// Delete email credentials
void deleteEmailCredentials() {
  LittleFS.remove(emailCredentialsPath);
}

String getSavedWiFiCredentials() {
  if (!LittleFS.exists(wifiCredentialsPath)) {
    return "No Wi-Fi credentials saved.";
  }

  File wifiFile = LittleFS.open(wifiCredentialsPath, "r");
  if (!wifiFile) {
    return "Error reading Wi-Fi credentials.";
  }

  String ssid = wifiFile.readStringUntil('\n');
  String password = wifiFile.readStringUntil('\n');
  wifiFile.close();


  String result = "SSID: " + ssid + "<br>Password: " + password;
  return result;
}

String getSavedEmailCredentials() {
  if (!LittleFS.exists(emailCredentialsPath)) {
    return "No email credentials saved.";
  }

  File emailFile = LittleFS.open(emailCredentialsPath, "r");
  if (!emailFile) {
    return "Error reading email credentials.";
  }

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, emailFile);
  emailFile.close();

  if (error) {
    return "Error parsing email credentials.";
  }

  String smtpServer = doc["smtpServer"].as<String>();
  String smtpUser = doc["smtpUser"].as<String>();
  String smtpSecurity = doc["smtpSecurity"].as<String>();
  String receiverMail = doc["receiverMail"].as<String>();

  // Return formatted credentials, hiding sensitive data
  String result = "SMTP Server: " + smtpServer + "<br>User: " + smtpUser +
                  "<br>Security: " + smtpSecurity + "<br>Receiver: " + receiverMail;
  return result;
}


bool readParametersFromFile(const char* path) {
  // Open the JSON file
  File file = LittleFS.open(path, "r");
  if (!file) {
    Serial.println("Failed to open file");
    return false;
  }

  // Determine the file size
  size_t size = file.size();
  if (size == 0) {
    Serial.println("File is empty");
    return false;
  }

  // Allocate a buffer to hold the file's content
  std::unique_ptr<char[]> buf(new char[size]);

  // Read the file into the buffer
  file.readBytes(buf.get(), size);

  // Parse the JSON
  StaticJsonDocument<512> jsonDoc;
  DeserializationError error = deserializeJson(jsonDoc, buf.get());

  serializeJson(jsonDoc, Serial);
  // Check for errors in parsing
  if (error) {
    Serial.print("Failed to parse JSON: ");
    Serial.println(error.c_str());
    return false;
  }

  // Extract the parameters and store them in variables
  lat = jsonDoc["lat"];
  lon = jsonDoc["lon"];
  year = jsonDoc["year"];
  peakpower = jsonDoc["peakpower"];
  loss = jsonDoc["loss"];
  pvtechchoice = jsonDoc["pvtechchoice"].as<String>();
  mountingplace = jsonDoc["mountingplace"].as<String>();
  angle = jsonDoc["angle"];
  aspect = jsonDoc["aspect"];
  age = jsonDoc["age"];
  degradation20Jahre = jsonDoc["degradation"];

  // Close the file
  file.close();
  
  return true;
}

void handleFileDownload(AsyncWebServerRequest *request) {
  String path = "/SensorData.csv"; // Change this to your file path

  if (LittleFS.exists(path)) {
    // Create a response directly from LittleFS without adding headers manually
    request->send(LittleFS, path, "application/octet-stream");
  } else {
    request->send(404, "text/plain", "File Not Found");
  }
}

#endif