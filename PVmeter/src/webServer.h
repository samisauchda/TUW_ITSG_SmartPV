#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
//#include <SPIFFS.h>
#include <esp_LittleFS.h>
#include <ESP_Mail_Client.h>

#include "helperFunctions.h"
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

// Create an instance of the server
AsyncWebServer credentialsServer(80);
AsyncWebServer server(80);

TaskHandle_t webServerTaskHandle = NULL;
TaskHandle_t credentialsTaskHandle = NULL;
TaskHandle_t smtpTaskHandle = NULL;

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


void startWebServer(const char* indexPage, const bool connected) {
  server.on("/", HTTP_GET, [indexPage](AsyncWebServerRequest *request) {
    request->send(LittleFS, indexPage, "text/html");
  });

  server.on("/add", HTTP_POST, [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    if (LittleFS.exists("/modules.json")) {
      File file = LittleFS.open("/modules.json", FILE_READ);
      deserializeJson(doc, file);
      file.close();
    }

    JsonArray modules = doc["modules"].as<JsonArray>();
    if (modules.isNull()) {
      modules = doc.createNestedArray("modules");
    }

    for (int i = 1; ; i++) {
      String nameParam = "name" + String(i);
      String maxPowerParam = "maxPower" + String(i);
      String angleParam = "angle" + String(i);
      String systemlossParam = "systemloss" + String(i);
      String slopeParam = "slope" + String(i);
      String azimuthParam = "azimuth" + String(i);
      String latParam = "lat" + String(i);
      String lonParam = "lon" + String(i);

      if (!request->hasParam(nameParam, true)) {
        break;
      }

      JsonObject module = modules.add<JsonObject>();
      if (request->hasParam(nameParam, true))
        module["name"] = request->getParam(nameParam, true)->value();
      if (request->hasParam(maxPowerParam, true))
        module["maxPower"] = request->getParam(maxPowerParam, true)->value().toFloat();
      if (request->hasParam(systemlossParam, true))
        module["systemloss"] = request->getParam(systemlossParam, true)->value().toInt();
      if (request->hasParam(slopeParam, true))
        module["slope"] = request->getParam(slopeParam, true)->value().toFloat();
      if (request->hasParam(azimuthParam, true))
        module["azimuth"] = request->getParam(azimuthParam, true)->value().toFloat();
      if (request->hasParam(latParam, true))
        module["lat"] = request->getParam(latParam, true)->value();
      if (request->hasParam(lonParam, true))
        module["lon"] = request->getParam(lonParam, true)->value();
    }

    File file = LittleFS.open("/modules.json", "w+");
    serializeJson(doc, file);
    file.close();

    request->send(200, "text/plain", "Module data saved");
  });

  // Route to handle Email Parameters
  server.on("/addEmailParams", HTTP_POST, [](AsyncWebServerRequest *request) {
    
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
    Serial.println("Parameters saved to /params.json");
    request->send(200, "text/plain", "Email params saved");

    setEmailParams(email["SMTPServer"], email["SMTPUser"],email["SMTPPass"], email["TLSPort"], email["SSLPort"], email["TLSSSL"], email["receiverMail"]);
    printEmailParams();
  });


  // Route to get the saved modules
  server.on("/modules", HTTP_GET, [](AsyncWebServerRequest *request) {
    File file = LittleFS.open("/modules.json", FILE_READ);
    String json;
    if (file) {
      json = file.readString();
      file.close();
    } else {
      json = "{\"modules\":[]}";
    }
    request->send(200, "application/json", json);
    //Serial.println(json);
  });

  server.on("/getEmailParams", HTTP_GET, [](AsyncWebServerRequest *request) {
    File file = LittleFS.open("/email.json", FILE_READ);
    String json;
    if (file) {
      json = file.readString();
      file.close();
    } else {
      json = "{\"email\":[]}";
    }
    request->send(200, "application/json", json);
    Serial.println(json);
  });

  // Route to delete a module
  server.on("/delete", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("index")) {
      int index = request->getParam("index")->value().toInt();
      JsonDocument doc;
      if (LittleFS.exists("/modules.json")) {
        File file = LittleFS.open("/modules.json", FILE_READ);
        deserializeJson(doc, file);
        file.close();
      }

      JsonArray modules = doc["modules"].as<JsonArray>();
      if (!modules.isNull() && index >= 0 && index < modules.size()) {
        modules.remove(index);

        File file = LittleFS.open("/modules.json", FILE_WRITE);
        serializeJson(doc, file);
        file.close();

        request->send(200, "text/plain", "Module deleted");
      } else {
        request->send(400, "text/plain", "Invalid module index");
      }
    } else {
      request->send(400, "text/plain", "Index parameter missing");
    }
  });

  server.on("/deleteAll", HTTP_POST, [](AsyncWebServerRequest *request){
    bool modulesDeleted;
    bool emailDeleted;
    bool wifiSSIDDeleted;
    bool wifiPASSDeleted;

    if( LittleFS.exists("/modules.json"))
      modulesDeleted = LittleFS.remove("/modules.json");
      modulesDeleted = !modulesDeleted;

    if( LittleFS.exists("/email.json"))
      emailDeleted = LittleFS.remove("/email.json");

    if( LittleFS.exists("/ssid.txt"))
      wifiSSIDDeleted = LittleFS.remove("/ssid.txt");

    if( LittleFS.exists("/password.txt"))
      wifiPASSDeleted = LittleFS.remove("/password.txt");

    char msgOut[1024]; 
    snprintf(msgOut, sizeof(msgOut), "Modules deleted: %s", modulesDeleted?"true":"false");
    //String sendText = "Modules deleted" + modulesDeleted + "\n" + "EMail Params deleted:" + emailDeleted + "\n" + "WiFi SSID deleted:" + wifiSSIDDeleted + "\n" + "WiFI Password deleted:" + wifiPASSDeleted + "\n";
    request->send(200, "text/plain", msgOut);
  });
  
  server.begin();
  Serial.println("HTTP server started");
}


void webServerTask(void * parameter) {
  
  if (ssid.length() > 0 && connectToWiFi(ssid.c_str(), password.c_str())) {
    // If credentials are available and connection is successful, start the server with index2.html
    startWebServer("/index2.html", 1);
    delay(1000);

    TaskParams task_params;
    task_params.message_to_send = WiFi.localIP().toString(); // 500 ms delay
    Serial.println(task_params.message_to_send);
    xTaskCreate(
      smtpTask,               // Function to implement the task
      "EmailTask",            // Name of the task
      10000,                  // Stack size in words
      &task_params,                   // Task input parameter
      1,                      // Priority of the task
      &smtpTaskHandle);       // Task handle
    

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

  for(;;) {
    // Allow the task to run indefinitely
    delay(1000);
    Serial.println(WiFi.status());
  }
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

  config.time.ntp_server = F("pool.ntp.org,time.nist.gov");
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


