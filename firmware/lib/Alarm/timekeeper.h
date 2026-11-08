// timekeeper.h - NTP (pool.ntp.org, America/New_York) with manual phone sync fallback.
// The ESP32 system clock free-runs off millis() even without NTP, so local time always "works";
// isSynced() tells you whether it's trustworthy.
#pragma once
#include <Arduino.h>
#include <time.h>

namespace TimeKeeper {
  void begin();                        // sets TZ and starts SNTP (needs WiFi to actually sync)
  void update(uint32_t now);           // logs the first successful sync
  bool isSynced();                     // NTP or manual sync happened
  void setManual(uint32_t epochUtc);   // phone pushes its time
  bool localTime(struct tm& out);      // local (NY) broken-down time
  String dateString();                 // YYYY-MM-DD (local)
  String timeString();                 // HH:MM:SS (local)
  String source();                     // "ntp" | "manual" | "millis"
}
