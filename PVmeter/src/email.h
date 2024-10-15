#ifndef EMAIL_H_
#define EMAIL_H_

#include <ESP_Mail_Client.h>
#include "LittleFS.h"
#include <stdio.h>
#include <map>

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

extern const char* emailCredentialsPath;

const char * emailTemplateIP = "This is the first email. Your IP address is: {IP}.";
const String emailTemplateWeekly = "This is the second email with 7 values:\nValue 1: {value1}\nValue 2: {value2}\nValue 3: {value3}\nValue 4: {value4}\nValue 5: {value5}\nValue 6: {value6}\nValue 7: {value7}.";
String emailText;
// empty char array for IP paramater handling
char ipStr[16]; // Allocate memory for the IP address string

extern float* PVData;
extern float* SensorMaxPower;



/* Declare the global used SMTPSession object for SMTP transport */
SMTPSession smtp;

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

String updateEmailText(bool breakdown, float Vergleichsfaktor1, float Vergleichsfaktor2, float Vergleichsfaktor3,
                       float Vergleichsfaktor4, float Vergleichsfaktor5, float Vergleichsfaktor6, float Vergleichsfaktor7,
                       float Energie1, float Energie2, float Energie3, float Energie4, float Energie5, float Energie6, 
                       float Energie7, float Altersfaktor) {
    // Create the email template
    String mailtext = 
        "Ergebnisse der Photovoltaik vom $Begin$ bis zum $End$\n"
        "Ergebnisse der Totalausfallerkennung: $breakdown$\n\n"
        "(Springt an, falls die PV-Anlage kaum/keine Leistung im Verhaeltnis zu den Vergleichswerten erzeugt hat.)\n\n"
        "\n\n"
        "Montag:      gemessene max. Leistung: $Energie1$ \n\n"
        "Dienstag:    Vergleichsfaktor: $Vergleichsfaktor2$, gemessene Leistung: $Energie2$ \n\n"
        "Mittwoch:    Vergleichsfaktor: $Vergleichsfaktor3$, gemessene Leistung: $Energie3$ \n\n"
        "Donnerstag:  Vergleichsfaktor: $Vergleichsfaktor4$, gemessene Leistung: $Energie4$ \n\n"
        "Freitag:     Vergleichsfaktor: $Vergleichsfaktor5$, Energie: $Energie5$ \n\n"
        "Samstag:     Vergleichsfaktor: $Vergleichsfaktor6$, Energie: $Energie6$ \n\n"
        "Sonntag:     Vergleichsfaktor: $Vergleichsfaktor7$, Energie: $Energie7$ \n\n"
        "\n\n"
        "(der vergleichswert ist die Differenz zu den Vergleichswerten multipliziert mit einem Altersfaktor [$Altersfaktor$].)\n\n";

    // Replace placeholders with actual values
    mailtext.replace("$breakdown$", breakdown ? "Ja" : "Nein");
    mailtext.replace("$Vergleichsfaktor1$", String(Vergleichsfaktor1));
    mailtext.replace("$Vergleichsfaktor2$", String(Vergleichsfaktor2));
    mailtext.replace("$Vergleichsfaktor3$", String(Vergleichsfaktor3));
    mailtext.replace("$Vergleichsfaktor4$", String(Vergleichsfaktor4));
    mailtext.replace("$Vergleichsfaktor5$", String(Vergleichsfaktor5));
    mailtext.replace("$Vergleichsfaktor6$", String(Vergleichsfaktor6));
    mailtext.replace("$Vergleichsfaktor7$", String(Vergleichsfaktor7));
    mailtext.replace("$Energie1$", String(Energie1));
    mailtext.replace("$Energie2$", String(Energie2));
    mailtext.replace("$Energie3$", String(Energie3));
    mailtext.replace("$Energie4$", String(Energie4));
    mailtext.replace("$Energie5$", String(Energie5));
    mailtext.replace("$Energie6$", String(Energie6));
    mailtext.replace("$Energie7$", String(Energie7));
    mailtext.replace("$Altersfaktor$", String(Altersfaktor));

    return mailtext;
}

// Function to send the email task with the constructed message
void sendEmailTaskWeekly(void *parameter) {
    loadEmailCredentials(); // Load email credentials from storage

    // Unpack the parameters from the void pointer (you can use a struct for better handling)
    float* values = (float*)parameter;
    bool breakdown = static_cast<bool>(values[0]); // Assuming breakdown is the first value
    float Vergleichsfaktor1 = values[1];
    float Vergleichsfaktor2 = values[2];
    float Vergleichsfaktor3 = values[3];
    float Vergleichsfaktor4 = values[4];
    float Vergleichsfaktor5 = values[5];
    float Vergleichsfaktor6 = values[6];
    float Vergleichsfaktor7 = values[7];
    float Energie1 = values[8];
    float Energie2 = values[9];
    float Energie3 = values[10];
    float Energie4 = values[11];
    float Energie5 = values[12];
    float Energie6 = values[13];
    float Energie7 = values[14];
    float Altersfaktor = values[15];

    // Construct the email text using the updateEmailText function
    String emailText = updateEmailText(breakdown, Vergleichsfaktor1, Vergleichsfaktor2, Vergleichsfaktor3,
                                        Vergleichsfaktor4, Vergleichsfaktor5, Vergleichsfaktor6, Vergleichsfaktor7,
                                        Energie1, Energie2, Energie3, Energie4, Energie5, Energie6, Energie7, Altersfaktor);
    
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