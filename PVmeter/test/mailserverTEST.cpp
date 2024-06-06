#include <Arduino.h>
#include <WiFi.h>
#include <list>

#include <ESPAsyncWebServer.h>
#include <ESP_Mail_Client.h>

const char* ssid = "HolLANd";
const char* password = "123polizei!";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// SMTP session config data
SMTPSession smtp;

// FreeRTOS task handles
TaskHandle_t smtpTaskHandle = NULL;
TaskHandle_t webServerTaskHandle = NULL;

void smtpCallback(SMTP_Status status) {
  // Handle the SMTP callback
  Serial.println(status.info());
}

void webServerTask(void * parameter) {
  // Define the web server routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", "Hello, world!");
  });

  // Start server
  server.begin();
  Serial.println("HTTP server started");

  for(;;) {
    // Allow the task to run indefinitely
    delay(1000);
  }
}

void smtpTask(void * parameter) {
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

  // Set the message content
  SMTP_Message message;
  message.sender.name = "ESP32";
  message.sender.email = "tuw.itsg.2024@gmail.com";
  message.subject = "Test Email";
  message.addRecipient("name1", "sam.auffenberg@gmail.com");
  message.text.content = "Hello from ESP32";

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

void setup() {
  Serial.begin(115200);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Start the web server task
  xTaskCreatePinnedToCore(
    webServerTask,        // Function to implement the task
    "WebServerTask",      // Name of the task
    10000,                // Stack size in words
    NULL,                 // Task input parameter
    1,                    // Priority of the task
    &webServerTaskHandle, // Task handle
    0);                   // Core where the task should run

  // Start the SMTP task
  xTaskCreatePinnedToCore(
    smtpTask,             // Function to implement the task
    "SMTPTask",           // Name of the task
    10000,                // Stack size in words
    NULL,                 // Task input parameter
    1,                    // Priority of the task
    &smtpTaskHandle,      // Task handle
    1);                   // Core where the task should run
}

void loop() {
  // Empty loop
}
