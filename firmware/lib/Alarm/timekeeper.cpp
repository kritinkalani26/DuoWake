#include "timekeeper.h"
#include "alarm_config.h"
#include <sys/time.h>
#include <esp_sntp.h>

namespace {
  constexpr time_t SANE_EPOCH = 1700000000;   // Nov 2023: anything earlier means "not synced"
  bool   _manual = false;
  bool   _loggedSync = false;
  uint32_t _lastCheck = 0;
}

void TimeKeeper::begin() {
  sntp_set_time_sync_notification_cb([](struct timeval*) { _manual = false; _loggedSync = false; });
  configTzTime(TZ_NEW_YORK, NTP_SERVER);
  Serial.printf("[time] TZ=%s ntp=%s\n", TZ_NEW_YORK, NTP_SERVER);
}

bool TimeKeeper::isSynced() { return time(nullptr) > SANE_EPOCH; }

void TimeKeeper::update(uint32_t now) {
  if (_loggedSync || now - _lastCheck < 1000) return;
  _lastCheck = now;
  if (isSynced()) {
    _loggedSync = true;
    Serial.printf("[time] synced via %s: %s %s\n", source().c_str(), dateString().c_str(), timeString().c_str());
  }
}

void TimeKeeper::setManual(uint32_t epochUtc) {
  struct timeval tv = { (time_t)epochUtc, 0 };
  settimeofday(&tv, nullptr);
  _manual = true;
  _loggedSync = false;
  Serial.printf("[time] manual sync -> %s %s\n", dateString().c_str(), timeString().c_str());
}

bool TimeKeeper::localTime(struct tm& out) {
  time_t t = time(nullptr);
  localtime_r(&t, &out);
  return isSynced();
}

String TimeKeeper::dateString() {
  struct tm tm; localTime(tm);
  char b[11]; strftime(b, sizeof(b), "%Y-%m-%d", &tm);
  return String(b);
}

String TimeKeeper::timeString() {
  struct tm tm; localTime(tm);
  char b[9]; strftime(b, sizeof(b), "%H:%M:%S", &tm);
  return String(b);
}

String TimeKeeper::source() {
  if (!isSynced()) return "millis";
  return _manual ? "manual" : "ntp";
}
