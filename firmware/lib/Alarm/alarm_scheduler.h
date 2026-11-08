// alarm_scheduler.h - user settings in NVS + "fire at HH:MM" logic + the prepared plan for tomorrow.
//
// Settings  = what the user chose on the phone (alarm time, game choice, tune choice, distance).
// Plan      = what will actually run (resolved game, resolved tune id, Gemini's reason). MASTER fills the
//             plan right after settings are saved (Gemini work happens then, never at alarm time).
#pragma once
#include <Arduino.h>

struct AlarmSettings {
  uint8_t hour = 7;
  uint8_t minute = 0;
  bool    enabled = true;
  String  game = "reaction";       // game id | "random" | "surprise"
  String  tune = "builtin:siren_stairs";  // builtin:<slug> | gen:<file> | new | shuffle
  float   distanceMeters = 5.0f;
  String  tuneStyle = "";          // free text for Gemini ("spooky", "8-bit boss fight"); only used for new melodies
};

struct AlarmPlan {
  String game;          // resolved game id
  String tune;          // resolved tune id (builtin:/gen:, or "shuffle" = pick at alarm time)
  String melodyName;    // display name
  String reason;        // Gemini's playful reason (or "" / local fallback text)
  String preparedFor;   // settings fingerprint this plan was built for
  bool   ready = false;
};

typedef void (*AlarmFireHandler)();

namespace AlarmScheduler {
  void begin();                              // loads settings + plan from NVS
  void update(uint32_t now);                 // fires once when local HH:MM matches (if enabled)
  void onFire(AlarmFireHandler h);

  AlarmSettings& settings();
  AlarmPlan&     plan();
  void saveSettings();                       // persist + invalidate plan
  void savePlan();
  String settingsFingerprint();              // changes whenever a plan-affecting setting changes
  bool   planIsCurrent();

  String nextAlarmString();                  // "Tomorrow 07:00" / "Today 07:00" / "off"
  time_t nextAlarmEpoch();                   // 0 if off
  String nextAlarmIso();                     // "YYYY-MM-DDTHH:MM" local, "" if off
  String alarmTimeString();                  // "HH:MM"
  void   fireNow();                          // "Test alarm now"
}
