#include "alarm_scheduler.h"
#include "timekeeper.h"
#include <Preferences.h>

namespace {
  AlarmSettings _s;
  AlarmPlan     _p;
  AlarmFireHandler _onFire = nullptr;
  String   _firedDate;        // local date we already fired on
  uint32_t _lastCheck = 0;

  void loadAll() {
    Preferences pr;
    if (!pr.begin("alarm", true)) return;
    _s.hour = pr.getUChar("hour", 7);
    _s.minute = pr.getUChar("min", 0);
    _s.enabled = pr.getBool("en", true);
    _s.game = pr.getString("game", "reaction");
    _s.tune = pr.getString("tune", "builtin:siren_stairs");
    _s.distanceMeters = pr.getFloat("dist", 5.0f);
    _s.tuneStyle = pr.getString("style", "");
    _p.game = pr.getString("pGame", "");
    _p.tune = pr.getString("pTune", "");
    _p.melodyName = pr.getString("pName", "");
    _p.reason = pr.getString("pReason", "");
    _p.preparedFor = pr.getString("pFor", "");
    _p.ready = pr.getBool("pReady", false);
    _firedDate = pr.getString("fired", "");
    pr.end();
  }
}

void AlarmScheduler::begin() {
  loadAll();
  Serial.printf("[alarm] settings: %02u:%02u %s game=%s tune=%s dist=%.1f m style=\"%s\"\n",
                _s.hour, _s.minute, _s.enabled ? "ON" : "off", _s.game.c_str(), _s.tune.c_str(), _s.distanceMeters, _s.tuneStyle.c_str());
  Serial.printf("[alarm] plan: %s game=%s tune=%s (%s) reason=\"%s\"\n",
                planIsCurrent() ? "ready" : "NOT READY", _p.game.c_str(), _p.tune.c_str(), _p.melodyName.c_str(), _p.reason.c_str());
}

AlarmSettings& AlarmScheduler::settings() { return _s; }
AlarmPlan& AlarmScheduler::plan() { return _p; }
void AlarmScheduler::onFire(AlarmFireHandler h) { _onFire = h; }

String AlarmScheduler::settingsFingerprint() {
  return _s.game + "|" + _s.tune + "|" + _s.tuneStyle;
}

bool AlarmScheduler::planIsCurrent() {
  return _p.ready && _p.preparedFor == settingsFingerprint();
}

void AlarmScheduler::saveSettings() {
  Preferences pr;
  if (pr.begin("alarm", false)) {
    pr.putUChar("hour", _s.hour);
    pr.putUChar("min", _s.minute);
    pr.putBool("en", _s.enabled);
    pr.putString("game", _s.game);
    pr.putString("tune", _s.tune);
    pr.putFloat("dist", _s.distanceMeters);
    pr.putString("style", _s.tuneStyle);
    pr.putString("fired", "");
    pr.end();
  }
  _firedDate = "";  // a freshly saved alarm may fire again today
  Serial.printf("[alarm] saved: %02u:%02u %s game=%s tune=%s dist=%.1f style=\"%s\"\n",
                _s.hour, _s.minute, _s.enabled ? "ON" : "off", _s.game.c_str(), _s.tune.c_str(), _s.distanceMeters, _s.tuneStyle.c_str());
}

void AlarmScheduler::savePlan() {
  Preferences pr;
  if (pr.begin("alarm", false)) {
    pr.putString("pGame", _p.game);
    pr.putString("pTune", _p.tune);
    pr.putString("pName", _p.melodyName);
    pr.putString("pReason", _p.reason);
    pr.putString("pFor", _p.preparedFor);
    pr.putBool("pReady", _p.ready);
    pr.end();
  }
  Serial.printf("[alarm] plan saved: game=%s tune=%s (%s) reason=\"%s\"\n",
                _p.game.c_str(), _p.tune.c_str(), _p.melodyName.c_str(), _p.reason.c_str());
}

String AlarmScheduler::nextAlarmString() {
  if (!_s.enabled) return "off";
  char hm[6]; snprintf(hm, sizeof(hm), "%02u:%02u", _s.hour, _s.minute);
  struct tm tm; TimeKeeper::localTime(tm);
  int nowMin = tm.tm_hour * 60 + tm.tm_min;
  int alarmMin = _s.hour * 60 + _s.minute;
  bool today = alarmMin > nowMin && _firedDate != TimeKeeper::dateString();
  return String(today ? "Today " : "Tomorrow ") + hm;
}

String AlarmScheduler::alarmTimeString() {
  char hm[6]; snprintf(hm, sizeof(hm), "%02u:%02u", _s.hour, _s.minute);
  return String(hm);
}

time_t AlarmScheduler::nextAlarmEpoch() {
  if (!_s.enabled) return 0;
  time_t now = time(nullptr);
  struct tm tm; localtime_r(&now, &tm);
  tm.tm_hour = _s.hour; tm.tm_min = _s.minute; tm.tm_sec = 0;
  time_t t = mktime(&tm);
  bool firedToday = _firedDate == TimeKeeper::dateString();
  if (t <= now || firedToday) { tm.tm_mday += 1; tm.tm_isdst = -1; t = mktime(&tm); }
  return t;
}

String AlarmScheduler::nextAlarmIso() {
  time_t t = nextAlarmEpoch();
  if (!t) return "";
  struct tm tm; localtime_r(&t, &tm);
  char b[17]; strftime(b, sizeof(b), "%Y-%m-%dT%H:%M", &tm);
  return String(b);
}

void AlarmScheduler::fireNow() {
  Serial.println("[alarm] FIRE (manual test)");
  if (_onFire) _onFire();
}

void AlarmScheduler::update(uint32_t now) {
  if (now - _lastCheck < 1000) return;
  _lastCheck = now;
  if (!_s.enabled) return;
  struct tm tm; TimeKeeper::localTime(tm);
  if (tm.tm_hour != _s.hour || tm.tm_min != _s.minute) return;
  String today = TimeKeeper::dateString();
  if (_firedDate == today) return;
  _firedDate = today;
  Preferences pr;
  if (pr.begin("alarm", false)) { pr.putString("fired", _firedDate); pr.end(); }
  Serial.printf("[alarm] FIRE at %s (time source: %s)\n", TimeKeeper::timeString().c_str(), TimeKeeper::source().c_str());
  if (_onFire) _onFire();
}
