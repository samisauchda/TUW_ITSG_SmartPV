#include <Arduino.h>
#include <WiFi.h>

#include <ESP_Mail_Client.h>

#define WIFI_SSID "HolLANd"
#define WIFI_PASSWORD "123polizei!"


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

void setup()
{
  Serial.begin(115200);
  Serial.println();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

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

void loop()
{
   Serial.print("Free heap: ");
  Serial.println(ESP.getFreeHeap());
  delay(100);
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
