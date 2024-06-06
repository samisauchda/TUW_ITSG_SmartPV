#include <ESP_Mail_Client.h>
#include <Arduino.h>

String smtpServer;
String smtpUser;
String smtpPass;
int smtpPortTLS;
int smtpPortSSL;
bool smtpTLSSSL;
String receiverMail;

SMTPSession smtp;
Session_Config config;

// Callback function to get the Email sending status
void smtpCallback(SMTP_Status status)
{
 
  Serial.println(status.info());


}

void initEmail() {

    smtp.debug(1);
    Serial.println("debug set");
    ESP_Mail_Session session;
    Serial.println("session initiated");

    // Set the session config
    config.server.host_name = smtpServer;
    config.server.port = 587; // for TLS with STARTTLS or 25 (Plain/TLS with STARTTLS) or 465 (SSL)
    config.login.email = smtpUser;
    config.login.password = smtpPass;
    Serial.println("session configured");
    // For client identity, assign invalid string can cause server rejection
    //config.login.user_domain = "client domain or public ip";  
    
    // Declare the SMTP_Message class variable to handle to message being transport
    SMTP_Message message;

    // Set the message headers
    message.sender.name = "My Mail";
    message.sender.email = smtpUser;
    message.subject = "Test sending Email";
    message.addRecipient("name1", receiverMail);

    // Set the message content
    message.text.content = "This is simple plain text message";
    Serial.println("message configured");


    // Set the callback function to get the sending results
    smtp.callback(smtpCallback);
    Serial.println("callback configured");
    // Connect to the server
    //smtp.connect(&config);

    Serial.println(smtp.connect(&config));


    // Start sending Email and close the session
    if (!MailClient.sendMail(&smtp, &message))
        Serial.println("Error sending Email, " + smtp.errorReason());
}


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

    initEmail();

}



