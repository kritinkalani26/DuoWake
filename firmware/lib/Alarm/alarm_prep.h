// alarm_prep.h - turns Settings into a ready Plan (MASTER only). Does the Gemini work right after the user
// saves, so the morning never waits on the network. If the alarm fires before the plan is ready (WiFi never
// came back) ensurePlanNow() builds a local fallback instantly without any network call.
#pragma once
#include <Arduino.h>

namespace AlarmPrep {
  void begin();
  void update(uint32_t now);         // runs the (blocking, seconds-long) Gemini prep when idle + WiFi up
  void requestPrep();                // call after settings change
  bool isPreparing();
  void ensurePlanNow();              // synchronous local fallback (no network) used at fire time
  String status();                   // human text for the UI
}
