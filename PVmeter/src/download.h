#ifndef DOWNLOAD_H_
#define DOWNLOAD_H_

#include <string.h> // Include for string handling
#include <ESPAsyncWebServer.h>
#include <esp_LittleFS.h>
#include <AsyncTCP.h>
#include <HTTPClient.h>

#include "helperFunctions.h"

extern double lat, lon, peakpower, loss, angle;
extern int year, aspect, age;
extern String pvtechchoice, mountingplace;

void downloadTask(void* parameter);
String buildURL(const String& tool_name);

void download(String host, String extension, AsyncClient* tcpClient, File* file);

AsyncClient tcpClient;
File file;



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




// Function to handle the download task
void downloadTask(void* parameter) {
  // Ensure memory usage is minimal within the task

  // Build the URL
  String url = buildURL("seriescalc");
  String filename = "/downloaded_file.txt";  // You can modify this logic if filename is dynamic


  
  if ((WiFi.status() == WL_CONNECTED)) {
    File file = LittleFS.open(filename.c_str(), FILE_WRITE);

    if (!file) {
        Serial.println("[LittleFS] Failed to open file for writing.");
        return;
    }

    HTTPClient http;

    USE_SERIAL.print("[HTTP] begin...\n");

    // configure server and url
    http.begin("http://192.168.0.125",5555);
    

    USE_SERIAL.print("[HTTP] GET...\n");
    // start connection and send HTTP header
    int httpCode = http.GET();
    if(httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);

        // file found at server
        if(httpCode == HTTP_CODE_OK) {

            // get length of document (is -1 when Server sends no Content-Length header)
            int len = http.getSize();

            // create buffer for read
            uint8_t buff[128] = { 0 };

            // get tcp stream
            WiFiClient * stream = http.getStreamPtr();

            // read all data from server
            while(http.connected() && (len > 0 || len == -1)) {
                // get available data size
                size_t size = stream->available();

                if(size) {
                    // read up to 128 byte
                    int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));

                    // write it to Serial
                    file.write(buff,c);
                    Serial.write(buff,c);
                    if(len > 0) {
                        len -= c;
                    }
                }
                delay(1);
            }

            USE_SERIAL.println();
            USE_SERIAL.print("[HTTP] connection closed or file end.\n");
            file.close();
            SD.end();

        }
    } else {
        USE_SERIAL.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();





    
  //https://github.com/me-no-dev/AsyncTCP/issues/110
    
    
  }

  // Delete the task to free its resources
  vTaskDelete(NULL);
}

void sendStringToServer( String sendMsg, AsyncClient* tcpClient ) {
  tcpClient->add( sendMsg.c_str() , sendMsg.length() );
  tcpClient->send();
}

static void handleData(void* arg, AsyncClient* client, void *data, size_t len) {
  static bool first_response = true; 
  File* file = (File*)arg;
  /* Server response information is being sent before the actual file. 
     This extra information should not be saved into the file.*/
  if (first_response) {
    size_t cur_pos = 0;
    char* temp = (char*) data;
    for (int i = 0; i < len; ++i) {
      if (temp[i] == '\n') {
        cur_pos = i;
      }
    }
    ++cur_pos;
    file->write((uint8_t*)data + cur_pos, len - cur_pos);
    first_response = false;
    return;
  }

  file->write((uint8_t*)data, len);
}

static void handleError(void* arg, AsyncClient* client, int8_t error) {
  Serial.printf("[CALLBACK] error %i \n", error);
  File* file = (File*)arg;
  file->close();
}

static void handleTimeOut(void* arg, AsyncClient* client, uint32_t time) {
  Serial.println("[CALLBACK] ACK timeout");
  File* file = (File*)arg;
  file->close();
}

static void handleDisconnect(void* arg, AsyncClient* client) {
  Serial.println("[CALLBACK] discconnected");
  File* file = (File*)arg;
  file->close();
}


void download(String host, String extension, AsyncClient* tcpClient, File* file) {
  // Assign callbacks
  tcpClient->onData(&handleData, file);
  tcpClient->onError(&handleError, file);
  tcpClient->onTimeout(&handleTimeOut, file);
  tcpClient->onDisconnect(&handleDisconnect, file);

  // Connect to host
  tcpClient->connect(host.c_str(), 80);
  while (!tcpClient->connected()) {
    Serial.print(".");
    delay(100);
  }
  Serial.println("Connected");

  // Generate TCP Download command
  String resp = String("GET ") +
                extension +
                String(" HTTP/1.1\r\n") +
                String("Host: ") +
                host +
                String("\r\n") +
                String("Icy-MetaData:1\r\n") +
                String("Connection: close\r\n\r\n");

  // Send download command
  sendStringToServer(resp, tcpClient);

  // After this point, onData Callback will receive the data and write it to SD card
}


#endif