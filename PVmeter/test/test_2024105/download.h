#ifndef DOWNLOAD_H_
#define DOWNLOAD_H_

#include <string.h> // Include for string handling
#include <ESPAsyncWebServer.h>
#include <esp_LittleFS.h>
#include <AsyncTCP.h>
#include <HTTPClient.h>


extern double lat, lon, peakpower, loss, angle;
extern int year, aspect, age;
extern String pvtechchoice, mountingplace;

void downloadTask(void* parameter);
String buildURL(const String& tool_name);



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
  String filename = "downloaded_file.txt";  // You can modify this logic if filename is dynamic


  
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


#endif