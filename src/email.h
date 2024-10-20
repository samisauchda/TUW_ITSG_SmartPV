#ifndef EMAIL_H_
#define EMAIL_H_

#include <ESP_Mail_Client.h>
#include "LittleFS.h"
#include <stdio.h>
#include <map>
#include <Arduino.h>
#include "helperFunctions.h"
#include "timeFunctions.h"


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

struct EmailParams {
    String subject;
    String message;
};

struct ergebnisTag {
  float diff_min;
  bool breakdown;
};

extern struct ergebnisTag ErgebnisWoche[7];

extern const char* emailCredentialsPath;

// Declare the variables as extern so that they can be used here
extern float lat, lon, peakpower, loss, angle, degradation20Jahre;
extern int year, aspect, age;
extern String pvtechchoice, mountingplace;

const char * emailTemplateIP = "PVMeter is setup and running. Your IP address is: %s";
const String emailTemplateWeekly = 
    "Aktuelle IP Adresse: $IP$\n\nErgebnisse der Photovoltaik vom $Begin$ bis zum $End$\n\nErgbenis der Totalausfallerkennung: $breakdown$\n(Springt an, falls die PV-Anlage kaum/keine Leistung im Verhaeltnis zu den Vergleichswerten erzeugt hat.)\n\n Montag:      Vergleichsfaktor: $Vergleichsfaktor0$\nDienstag:    Vergleichsfaktor: $Vergleichsfaktor1$ \nMittwoch:    Vergleichsfaktor: $Vergleichsfaktor2$ \nDonnerstag:  Vergleichsfaktor: $Vergleichsfaktor3$ \nFreitag:     Vergleichsfaktor: $Vergleichsfaktor4$ \nSamstag:     Vergleichsfaktor: $Vergleichsfaktor5$ \nSonntag:     Vergleichsfaktor: $Vergleichsfaktor6$ \n\n(der Vergleichsfaktor ist die Differenz zu den Vergleichswerten multipliziert mit einem Altersfaktor[$Altersfaktor$].)\n";

// empty char array for IP paramater handling
char ipStr[16]; // Allocate memory for the IP address string

extern float* PVData;
extern float* SensorMaxPower;



/* Declare the global used SMTPSession object for SMTP transport */
SMTPSession smtp;

extern bool check_breakdown(struct ergebnisTag ErgebnisWoche[7]);
extern float get_Altersfaktor(float given_faktor, int alter);

void sendEmailTask(void *parameter);
void sendEmailTaskIPaddress(void *parameter);
void smtpCallback(SMTP_Status status);
void sendEmail( const char* subject, 
                const char* textMsg, 
                const char* host, 
                int port, 
                const char* email, 
                const char* password, 
                const char* domain, 
                const char* senderName, 
                const char* recipientEmail);
bool loadEmailCredentials();

// task to send test Mail
void sendEmailTask(void *parameter) {
    loadEmailCredentials(); // Load email credentials from storage

    // Extract the configuration items from emailCreds
    const char* subject = "Test sending a message";
    const char* textMsg = "This is a simple plain text message\n";
    const char* host = emailCreds.smtpServer.c_str(); // Get SMTP server
    int port = emailCreds.smtpPortTLS;                // Use TLS port or change as needed
    const char* email = emailCreds.smtpUser.c_str();  // SMTP user email
    const char* password = emailCreds.smtpPass.c_str(); // SMTP password
    const char* domain = "127.0.0.1";                   // Adjust if needed
    const char* senderName = "PVMeter";
    const char* recipientEmail = emailCreds.receiverMail.c_str(); // Get receiver email

    // Call sendEmail with the extracted parameters
    sendEmail(subject, textMsg, host, port, email, password, domain, senderName, recipientEmail);

    Serial.println("Email sent, task completed.");
    vTaskDelete(NULL);  // Task deleted after email sent
}

// Task to send Email with IP upon startup
void sendEmailTaskIPaddress(void *parameter) {
    loadEmailCredentials(); // Load email credentials from storage

    // Extract the current IP address passed as a parameter
    const char* currentIPAddress = (const char*)parameter;

    // Create a message text with the IP address
    const char* subject = "Deine PVMeter IP Adresse.";
    
    // Replace {IP} with the actual IP address in the message
    char textMsg[200]; // Adjust size as needed
    snprintf(textMsg, sizeof(textMsg), emailTemplateIP, currentIPAddress);
    
    const char* host = emailCreds.smtpServer.c_str(); // Get SMTP server
    int port = emailCreds.smtpPortTLS;                // Use TLS port or change as needed
    const char* email = emailCreds.smtpUser.c_str();  // SMTP user email
    const char* password = emailCreds.smtpPass.c_str(); // SMTP password
    const char* domain = "127.0.0.1";                   // Adjust if needed
    const char* senderName = "PVMeter";
    const char* recipientEmail = emailCreds.receiverMail.c_str(); // Get receiver email

    // Call sendEmail with the extracted parameters
    sendEmail(subject, textMsg, host, port, email, password, domain, senderName, recipientEmail);

    Serial.println("Email sent, task completed.");
    vTaskDelete(NULL);  // Task deleted after email sent
}

// Function to send an email
void sendEmail( const char* subject, 
                const char* textMsg, 
                const char* host, 
                int port, 
                const char* email, 
                const char* password, 
                const char* domain, 
                const char* senderName, 
                const char* recipientEmail) {
    smtp.callback(smtpCallback);

    Session_Config config;
    config.server.host_name = host;          // Use the passed host
    config.server.port = port;                // Use the passed port
    config.login.email = email;               // Use the passed email
    config.login.password = password;         // Use the passed password
    config.login.user_domain = F(domain);     // Use the passed domain

    SMTP_Message message;
    message.sender.name = F(senderName);      // Use the passed sender name
    message.sender.email = email;              // Use the same email for sender

    message.subject = F(subject);              // Use the passed subject
    message.addRecipient(F("Empfänger"), recipientEmail); // Use the passed recipient name and email

    // Use the passed text message
    message.text.content = textMsg;
    message.text.transfer_encoding = "base64";  // Encoding for non-ASCII text.
    message.text.charSet = F("utf-8");
    message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_low;

    message.addHeader(F("Message-ID: <abcde.fghij@gmail.com>"));

    if (!smtp.connect(&config)) {
        Serial.printf("Connection error, Status Code: %d, Error Code: %d, Reason: %s\n", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
        return;
    }

    if (!smtp.isLoggedIn()) {
        Serial.println("Not yet logged in.");
    } else {
        Serial.println(smtp.isAuthenticated() ? "Successfully logged in." : "Connected with no Auth.");
    }

    if (!MailClient.sendMail(&smtp, &message)) {
        Serial.printf("Error, Status Code: %d, Error Code: %d, Reason: %s\n", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
    }

    smtp.sendingResult.clear();  // Clear result after sending
}

// Callback to report email status
void smtpCallback(SMTP_Status status) {
    Serial.println(status.info());

    if (status.success()) {
        Serial.println("----------------");
        Serial.printf("Message sent success: %d\n", status.completedCount());
        Serial.printf("Message sent failed: %d\n", status.failedCount());
        Serial.println("----------------\n");

        for (size_t i = 0; i < smtp.sendingResult.size(); i++) {
            SMTP_Result result = smtp.sendingResult.getItem(i);

            Serial.printf("Message No: %d\n", i + 1);
            Serial.printf("Status: %s\n", result.completed ? "success" : "failed");
            Serial.printf("Date/Time: %s\n", MailClient.Time.getDateTimeString(result.timestamp, "%B %d, %Y %H:%M:%S").c_str());
            Serial.printf("Recipient: %s\n", result.recipients.c_str());
            Serial.printf("Subject: %s\n", result.subject.c_str());
        }

        Serial.println("----------------\n");

        smtp.sendingResult.clear();  // Free up memory
    }
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

// Function to send the email task with the constructed message
void sendEmailTaskWeekly(void *parameter) {
    loadEmailCredentials(); // Load email credentials from storage



    bool isBreakdown = check_breakdown(ErgebnisWoche);
    // breakdown aus allen Tagen muss kombiniert werden zu einem
    String emailText = emailTemplateWeekly;

    IPAddress ip = WiFi.localIP();
    String ipStr = ipToString(ip);

    // Create a buffer to store formatted strings
    char dateStr[20];  // String buffer for formatted date (DD-MM-YYYY)
    
    // Calculate yesterday's date
    time_t now;
    time(&now);  // Get current time as time_t
    now -= 86400;  // Subtract 1 day
    struct tm *yesterday = localtime(&now);
    
    // Format yesterday's date to string
    strftime(dateStr, sizeof(dateStr), "%d-%m-%Y", yesterday);
    String yesterdayStr = String(dateStr);  // Convert to String class
    Serial.println("Yesterday's Date: " + yesterdayStr);
    
    // Calculate the date of one week ago
    time(&now);  // Reset now to current time
    now -= 7 * 86400;  // Subtract 7 days
    struct tm *weekAgo = localtime(&now);
    
    // Format one week ago date to string
    strftime(dateStr, sizeof(dateStr), "%d-%m-%Y", weekAgo);
    String weekAgoStr = String(dateStr);  // Convert to String class
    Serial.println("Date one week ago: " + weekAgoStr);
    

    emailText.replace("$IP$",ipStr);

    emailText.replace("$breakdown$", isBreakdown ? "true" : "false");
    emailText.replace("$Begin$", weekAgoStr);
    emailText.replace("$End$", yesterdayStr);
    emailText.replace("$Altersfaktor$", String(get_Altersfaktor(degradation20Jahre, age)));

    emailText.replace("$Vergleichsfaktor0$", String(ErgebnisWoche[0].diff_min));
    emailText.replace("$Vergleichsfaktor1$", String(ErgebnisWoche[1].diff_min));
    emailText.replace("$Vergleichsfaktor2$", String(ErgebnisWoche[2].diff_min));
    emailText.replace("$Vergleichsfaktor3$", String(ErgebnisWoche[3].diff_min));
    emailText.replace("$Vergleichsfaktor4$", String(ErgebnisWoche[4].diff_min));
    emailText.replace("$Vergleichsfaktor5$", String(ErgebnisWoche[5].diff_min));
    emailText.replace("$Vergleichsfaktor6$", String(ErgebnisWoche[6].diff_min));
    
    const char* subject = "Ergebnisse der Photovoltaikanlage";
    const char* host = emailCreds.smtpServer.c_str(); // Get SMTP server
    int port = emailCreds.smtpPortTLS;                // Use TLS port or change as needed
    const char* email = emailCreds.smtpUser.c_str();  // SMTP user email
    const char* password = emailCreds.smtpPass.c_str(); // SMTP password
    const char* domain = "127.0.0.1";                   // Adjust if needed
    const char* senderName = "PVMeter";
    const char* recipientEmail = emailCreds.receiverMail.c_str(); // Get receiver email

    // Call sendEmail with the extracted parameters
    sendEmail(subject, emailText.c_str(), host, port, email, password, domain, senderName, recipientEmail);


    Serial.println("Email sent, task completed.");
    vTaskDelete(NULL);  // Task deleted after email sent
}

#endif