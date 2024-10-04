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

// Network credentials for Access Point
const char* ssidAP = "ESP32-Access-Point";
const char* passwordAP = "12345678";

// Variables to store credentials
String ssid = "";
String password = "";

// File paths to save credentials
const char* ssidPath = "/ssid.txt";
const char* passwordPath = "/password.txt";

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

TaskHandle_t webServerTaskHandle = NULL;
TaskHandle_t credentialsTaskHandle = NULL;
TaskHandle_t smtpTaskHandle = NULL;
TaskHandle_t downloadFileHandle = NULL;

TimerHandle_t timerHandleStopWebserver = NULL;

String smtpServer;
String smtpUser;
String smtpPass;
int smtpPortTLS;
int smtpPortSSL;
bool smtpTLSSSL;
String receiverMail;

SMTPSession smtp;
Session_Config config;

typedef struct {
    String message_to_send;
} TaskParams;

void webServerTask(void * parameter);
void credentialsTask(void * parameter);
void smtpTask(void * parameter);

void downloadFileToLittleFS(const String &filepath, const String &url);
String readParameters();
void saveParameters(AsyncWebServerRequest *request);
void deleteParameters();
String readHTMLFile();

// Forward declare the function to read parameters
bool readParametersFromFile(const char* path);
String buildURL(const String& tool_name);

void startDownloadTask(const String &filepath, const String &url);
void downloadTask(void *parameter);

extern float* PVData;


void setEmailParams(String newSmtpServer, 
                    String newSmtpUser, 
                    String newSmtpPass, 
                    int newSmtpPortTLS, 
                    int newSmtpPortSSL,
                    bool newSmtpTLSSSL,
                    String newreceiverMail) {
    smtpServer = newSmtpServer;
    smtpUser = newSmtpUser;
    smtpPass = newSmtpPass;
    smtpPortTLS = newSmtpPortTLS;
    smtpPortSSL = newSmtpPortSSL;
    smtpTLSSSL = newSmtpTLSSSL;
    receiverMail = newreceiverMail;
}

// Read credentials from LittleFS
bool readCredentials() {
  if (LittleFS.exists(ssidPath)) {
    File file = LittleFS.open(ssidPath, "r");
    if (file) {
      ssid = file.readStringUntil('\n');
      ssid.trim();
      file.close();
    }
  } else {
    return false;
  }

  if (LittleFS.exists(passwordPath)) {
    File file = LittleFS.open(passwordPath, "r");
    if (file) {
      password = file.readStringUntil('\n');
      password.trim();
      file.close();
    }
  } else {
    return false;
  }

  return true;
}

// Save credentials to LittleFS
void saveCredentials(const char* ssid, const char* password) {
  File file = LittleFS.open(ssidPath, "w");
  if (file) {
    file.println(ssid);
    file.close();
  }

  file = LittleFS.open(passwordPath, "w");
  if (file) {
    file.println(password);
    file.close();
  }
}

// Connect to Wi-Fi with provided credentials
bool connectToWiFi(const char* ssid, const char* password) {
  Serial.printf("Connecting to %s", ssid);
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
  // server.on("/", HTTP_GET, [htmlPage](AsyncWebServerRequest *request) {
  //   request->send(LittleFS, htmlPage, "text/html");
  // });

  // Serve HTML form
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String savedParams = readParameters();
    String htmlContent = readHTMLFile();
    htmlContent.replace("%SAVEDPARAMS%", savedParams);
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
  
  if (ssid.length() > 0 && connectToWiFi(ssid.c_str(), password.c_str())) {
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
  Serial.println(WiFi.softAPIP());

  //startCredentialsServer("/index.html");

  credentialsServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
  });

  // handles for credential submission on AP mode
  credentialsServer.on("/get", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("ssid") && request->hasParam("password")) {
      ssid = request->getParam("ssid")->value();
      password = request->getParam("password")->value();
      saveCredentials(ssid.c_str(), password.c_str());

      String serverParam = "SMTPserver";
      String userParam = "SMTPuser";
      String passParam = "SMTPpass";
      String tlsPortParam = "SMTPportTLS";
      String sslPortParam = "SMTPportSSL";
      String TLSSSLParam = "TLSSSL";
      String receiverMailParam = "receiverMAIL";

      //JsonObject email = emails.createNestedObject();
      JsonDocument email;
      if (request->hasParam(serverParam, true))
        email["SMTPServer"] = request->getParam(serverParam, true)->value();
      if (request->hasParam(userParam, true))
        email["SMTPUser"] = request->getParam(userParam, true)->value();
      if (request->hasParam(passParam, true))
        email["SMTPPass"] = request->getParam(passParam, true)->value();
      if (request->hasParam(tlsPortParam, true))
        email["TLSPort"] = request->getParam(tlsPortParam, true)->value().toInt();
      if (request->hasParam(sslPortParam, true))
        email["SSLPort"] = request->getParam(sslPortParam, true)->value().toInt();
      if (request->hasParam(TLSSSLParam, true))
        email["TLSSSL"] = true;
      else
        email["TLSSSL"] = false;
      if (request->hasParam(receiverMailParam, true))
        email["receiverMail"] = request->getParam(receiverMailParam, true)->value();

      //LittleFS.remove("/email.json");
      
      File file = LittleFS.open("/email.json", FILE_WRITE);
      if (!file) {
        Serial.println("Failed to open file for writing");
        return;
      }
      serializeJson(email, file);
      file.close();
      Serial.println("Parameters saved to /email.json");
      request->send(200, "text/plain", "Email params saved");

      setEmailParams(email["SMTPServer"], email["SMTPUser"],email["SMTPPass"], email["TLSPort"], email["SSLPort"], email["TLSSSL"], email["receiverMail"]);
      printEmailParams();

      request->send(200, "text/html", "Credentials received, attempting to connect...");
      credentialsServer.end();
      delay(1000);
      WiFi.softAPdisconnect(true);
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
      
    } else {
      request->send(200, "text/html", "Missing SSID or Password.");
    }
  });

  credentialsServer.begin();
  Serial.println("HTTP server started");


  for(;;) {
    // Allow the task to run indefinitely
    delay(10);
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

String readHTMLFile() {
  String htmlContent;
  File file = LittleFS.open("/index2.html", "r");
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