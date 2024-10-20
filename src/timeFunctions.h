#ifndef TIMEFUNCTIONS_H_
#define TIMEFUNCTIONS_H_


#include <ESP32Time.h>



//ESP32Time rtc;
ESP32Time rtc(3600);  // offset in seconds GMT+1
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;
const int   maxClockSyncRetry = 5;
const int   clockRetryDelay = 5000;

// calculates index in Data Arrays for current time
int calculateDataIndex() {
  int dayOfYear = rtc.getDayofYear();
  
  // Get the current hour (0-23)
  int hour = rtc.getHour();

  int index = dayOfYear * 24 + hour;
  return index;
}

// returns current time as string
String getFormattedTime() {
    
    char timeBuffer[14];


    Serial.println(rtc.getTime("%Y%m%d:%H%M"));

    return String("Hello");
}

// syncs RTC time with NTP Server
void syncRTCtime() {
  int attempt = 0;
  bool success = false;

  struct tm timeinfo;

  while (attempt < maxClockSyncRetry && !success) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    if (getLocalTime(&timeinfo)) {
      rtc.setTimeStruct(timeinfo);
      Serial.print("Time synced with NTP: ");
      Serial.println(rtc.getTime());
      success = true;
    } else {
      printf("Failed to obtain time, retrying %i/%i... \n", attempt, maxClockSyncRetry);
      attempt++;
      delay(clockRetryDelay);
    }
  }

  if (!success) {
    Serial.println("Failed to sync time after maximum retries.");
  }
}


#endif