#ifndef WEBSERVER_H_
#define WEBSERVER_H_


#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
//#include <SPIFFS.h>
#include <esp_LittleFS.h>
#include <ESP_Mail_Client.h>
#include <HTTPClient.h>
#include "helperFunctions.h"
#include "csvFunctions.h"
#include "Sensor.h"
// #include "email.h"
#include <DNSServer.h>

// Network credentials for Access Point
const char* ssidAP = "ESP32-Access-Point";
const char* passwordAP = "12345678";

// Variables to store credentials
String ssid = "";
String password = "";

// File paths to save credentials
// const char* ssidPath = "/ssid.txt";
// const char* passwordPath = "/password.txt";

// File to store parameters
extern const char* parameterFile;


// Declare the variables as extern so that they can be used here
extern double lat, lon, peakpower, loss, angle;
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

// Structure to store email credentials
struct EmailCredentials {
  String smtpServer;
  String smtpUser;
  String smtpPass;
  int smtpPortTLS;
  int smtpPortSSL;
  String smtpSecurity;
  String receiverMail;
};
// Initialize email credentials
extern EmailCredentials emailCreds;



SMTPSession smtp;
Session_Config config;

typedef struct {
    String message_to_send;
} TaskParams;

void webServerTask(void * parameter);
void credentialsTask(void * parameter);
void smtpTask(void * parameter);

bool connectToWiFi();

void downloadFileToLittleFS(const String &filepath, const String &url);
String readParameters();
void saveParameters(AsyncWebServerRequest *request);
void deleteParameters();
String readHTMLFile(const char * filePath);

// Forward declare the function to read parameters
bool readParametersFromFile(const char* path);
String buildURL(const String& tool_name);

void startDownloadTask(const String &filepath, const String &url);
void downloadTask(void *parameter);

extern float* PVData;

void saveWiFiCredentials(String ssid, String password);
void saveEmailCredentials(EmailCredentials creds);
bool loadEmailCredentials();
void deleteWiFiCredentials();
void deleteEmailCredentials();
String getSavedWiFiCredentials();
String getSavedEmailCredentials(); 


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
    return true;
  } else {
    Serial.println("Failed to connect to WiFi");
    return false;
  }
}


void startWebServer(const char* htmlPage, const bool connected) {
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

  // Handle test function call
  server.on("/downloadFile", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("Calling downloadFile function");
        String downloadURL = buildURL("seriescalc");
        Serial.println(downloadURL);
        
        // Start the download task
        startDownloadTask("/hourly_data.csv", downloadURL);

        // Respond immediately to the client
        request->send(200, "text/plain", "Download started.");
    });

  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {},
    [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
      if (!index) {
        Serial.printf("UploadStart: %s\n", filename.c_str());
        // Open file for writing (create if it doesn't exist)
        File file = LittleFS.open("/hourlyData.csv", FILE_WRITE);
        if (!file) {
          Serial.println("Failed to open file for writing");
          return;
        }
        file.close();
      }

      // Open file in append mode and write to it
      File file = LittleFS.open("/hourlyData.csv", FILE_APPEND);
      if (file) {
        if (file.write(data, len) != len) {
          Serial.println("Failed to write file");
        }
        file.close();
      }

      if (final) {
        Serial.printf("UploadEnd: %s, %u B\n", filename.c_str(), index + len);
        request->send(200, "text/plain", "File Uploaded Successfully!");
      }
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
  server.on("/deleteWifi", HTTP_GET, [](AsyncWebServerRequest *request){
    deleteWiFiCredentials();
    request->redirect("/");
  });

  // Delete email credentials
  server.on("/deleteEmail", HTTP_GET, [](AsyncWebServerRequest *request){
    deleteEmailCredentials();
    request->redirect("/");
  });

  server.begin();
  Serial.println("HTTP server started");
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
            
          }
        }, "SensorTask", 20000, NULL, 1, &sensorTaskHandle);
        return;
    }
    Serial.println("WebServer task not started or already killed. Doing nothing...");
}


void webServerTask(void * parameter) {
  
  if (connectToWiFi()) {
    // If credentials are available and connection is successful, start the server with index2.html
    startWebServer("/index2.html", 1);
    delay(1000);

    TaskParams task_params;
    task_params.message_to_send = WiFi.localIP().toString(); // 500 ms delay
    Serial.println(task_params.message_to_send);
    // xTaskCreate(
    //   smtpTask,               // Function to implement the task
    //   "EmailTask",            // Name of the task
    //   10000,                  // Stack size in words
    //   &task_params,                   // Task input parameter
    //   1,                      // Priority of the task
    //   &smtpTaskHandle);       // Task handle
    

  } else {
    Serial.println("Wrong credentials or WiFi not found");
    
    xTaskCreate(
        credentialsTask,        // Function to implement the task
        "CredentialsTask",      // Name of the task
        10000,                // Stack size in words
        NULL,                 // Task input parameter
        1,                    // Priority of the task
        &credentialsTaskHandle); // Task handle
    Serial.println("credentials Task Started.");
    vTaskDelete(webServerTaskHandle);
    Serial.println("deleted WebServer Task. Starting Credentials task");
  }

  //vTaskDelete(webServerTaskHandle);
  for(;;) {
    // Allow the task to run indefinitely
    delay(10);
    //Serial.println(WiFi.status());
  }
}

void startDownloadTask(const String &filepath, const String &url) {
    String *params = new String[2];
    params[0] = filepath;
    params[1] = url;

    // Create a new task to handle the file download
    xTaskCreate(downloadTask, "DownloadTask", 8192, params, 1, NULL);
}

void downloadTask(void *parameter) {
    String *params = static_cast<String *>(parameter);
    String filepath = params[0];
    String url = params[1];

    downloadFileToLittleFS(filepath, url);
    listLittleFSFiles();


    // Delete parameters after use
    delete[] params; 
    vTaskDelete(NULL);  // End the task
}


void credentialsTask(void * parameter){
  Serial.println("Credentials Task started");
  // If no credentials or failed connection, start the Access Point and serve index.html
  Serial.print("No credentials available. Starting AP... ");
  WiFi.softAP(ssidAP, passwordAP);
  Serial.println("Access Point started");
  Serial.print("IP Address: ");
    // Setze die IP-Adresse des ESP32 im Access Point-Modus
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

  // Starte den DNS-Server und leite alle Anfragen auf die IP des ESP32 um
  // dnsServer.start(53, "*", apIP);

  //startCredentialsServer("/index.html");

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
    vTaskDelete(credentialsTaskHandle);
    Serial.println("deleted credentials Task. Starting WebServer task");
    xTaskCreate(
      webServerTask,        // Function to implement the task
      "WebServerTask",      // Name of the task
      10000,                // Stack size in words
      NULL,                 // Task input parameter
      1,                    // Priority of the task
      &webServerTaskHandle); // Task handle
    Serial.println("WebServer Task Started.");
  });

  credentialsServer.begin();
  Serial.println("HTTP server started");


  for(;;) {
    // Allow the task to run indefinitely
     // Lasse den DNS-Server laufen
    // dnsServer.processNextRequest();
  }
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

void downloadFileToLittleFS(const String &filepath, const String &url) {
    // Wait for WiFi connection
    if ((WiFi.status() == WL_CONNECTED)) {
        File file = LittleFS.open(filepath.c_str(), FILE_WRITE);

        if (!file) {
            Serial.println("[LittleFS] Failed to open file for writing.");
            return;
        }

        HTTPClient http;

        Serial.print("[HTTP] begin...\n");

        // Configure the server and URL (no port parameter needed)
        http.begin(url);

        Serial.print("[HTTP] GET...\n");
        // Start connection and send HTTP header
        int httpCode = http.GET();

        if (httpCode > 0) {
            // HTTP header has been sent and Server response header has been handled
            Serial.printf("[HTTP] GET... code: %d\n", httpCode);

            // File found at server
            if (httpCode == HTTP_CODE_OK) {
                // Get length of the document (is -1 when Server sends no Content-Length header)
                int len = http.getSize();

                // Create buffer for reading
                uint8_t buff[128] = { 0 };

                // Get TCP stream
                WiFiClient *stream = http.getStreamPtr();

                // Read all data from server
                while (http.connected() && (len > 0 || len == -1)) {
                    // Get available data size
                    size_t size = stream->available();

                    if (size) {
                        // Read up to 128 bytes
                        int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));

                        // Write it to LittleFS file
                        file.write(buff, c);
                        // Optionally, write to Serial for debugging
                        //Serial.write(buff, c);

                        if (len > 0) {
                            len -= c;
                        }
                    }
                    delay(1);
                }

                Serial.println();
                Serial.print("[HTTP] connection closed or file end.\n");
                file.close();
            }
        } else {
            Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
        }

        http.end();
    }
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

// Save email credentials to a JSON file
void saveEmailCredentials(EmailCredentials creds) {
  File emailFile = LittleFS.open(emailCredentialsPath, "w");
  if (!emailFile) {
    Serial.println("Failed to open email credentials file for writing");
    return;
  }

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

// Load email credentials from the JSON file
bool loadEmailCredentials() {
  File emailFile = LittleFS.open(emailCredentialsPath, "r");
  if (!emailFile) {
    Serial.println("Email credentials file not found");
    return false;
  }

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, emailFile);
  emailFile.close();
  
  if (error) {
    Serial.println("Failed to parse email credentials");
    return false;
  }

  emailCreds.smtpServer = doc["smtpServer"].as<String>();
  emailCreds.smtpUser = doc["smtpUser"].as<String>();
  emailCreds.smtpPass = doc["smtpPass"].as<String>();
  emailCreds.smtpPortTLS = doc["smtpPortTLS"];
  emailCreds.smtpPortSSL = doc["smtpPortSSL"];
  emailCreds.smtpSecurity = doc["smtpSecurity"].as<String>();
  emailCreds.receiverMail = doc["receiverMail"].as<String>();

  return true;
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


void smtpCallback(SMTP_Status status) {
  // Handle the SMTP callback
  Serial.println(status.info());
}

void smtpTask(void * parameter) {
  TaskParams *task_params = (TaskParams *)parameter;
  String message_to_send = task_params->message_to_send;

  // Configure the SMTP session
  smtp.debug(1);
  smtp.callback(smtpCallback);

  // Set SMTP server settings
  Session_Config config;
  config.server.host_name = "smtp.gmail.com";
  config.server.port = 465;
  config.login.email = "tuw.itsg.2024@gmail.com";
  config.login.password = "rbxpegoflnsownim";
  config.login.user_domain = "";

  config.time.ntp_server = F("at.pool.ntp.org,de.pool.ntp.org,pool.ntp.org");
  config.time.gmt_offset = 3;
  config.time.day_light_offset = 0;

  Serial.println("Message to send:");
  Serial.println(message_to_send);
  // Set the message content
  SMTP_Message message;
  message.sender.name = "ESP32";
  message.sender.email = "tuw.itsg.2024@gmail.com";
  message.subject = "Test Email";
  message.addRecipient("name1", "sam.auffenberg@gmail.com");
  message.text.content = message_to_send;

  /* Connect to server with the session config */
  if (!smtp.connect(&config)){
    ESP_MAIL_PRINTF("Connection error, Status Code: %d, Error Code: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
    return;
  }

  if (!smtp.isLoggedIn()){
    Serial.println("\nNot yet logged in.");
  }
  else{
    if (smtp.isAuthenticated())
      Serial.println("\nSuccessfully logged in.");
    else
      Serial.println("\nConnected with no Auth.");
  }

  // Send the email
  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.println("Error sending Email, " + smtp.errorReason());
  } else {
    Serial.println("Email sent successfully");
  }

  // Delete the task after email is sent
  vTaskDelete(NULL);
}


// Function to build the URL
String buildURL(const String& tool_name) {
    String url = "https://re.jrc.ec.europa.eu/api/v5_3/" + tool_name + "?";

    bool firstParam = true;  // To manage the first parameter addition

    // Helper function to append parameters to the URL
    auto appendParam = [&](const String& key, const String& value) {
        if (!value.isEmpty()) {
            if (firstParam) {
                url += key + "=" + value;
                firstParam = false;  // No longer the first parameter
            } else {
                url += "&" + key + "=" + value;
            }
        }
    };


    // Extract parameters from JSON document and append them to the URL
    appendParam("lat", String(lat));
    appendParam("lon", String(lon));
    appendParam("startyear", String(year)); // Use startyear
    appendParam("endyear", String(year));   // Use endyear
    appendParam("peakpower", String(peakpower));
    appendParam("loss", String(loss));
    appendParam("pvtechchoice", String(pvtechchoice));
    appendParam("mountingplace", String(mountingplace));
    appendParam("angle", String(angle));
    appendParam("aspect", String(aspect));
    appendParam("pvcalculation", String(1));

    return url;  // Return the constructed URL
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

  // Close the file
  file.close();
  
  return true;
}


#endif