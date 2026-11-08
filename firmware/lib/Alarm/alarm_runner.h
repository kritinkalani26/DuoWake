// alarm_runner.h - runs one alarm session on MASTER: loads the melody, creates the game from the
// registry, feeds it buttons/updates until complete, then celebrates and goes idle.
#pragma once
#include <Arduino.h>
#include "alarm_scheduler.h"

namespace AlarmRunner {
  void begin();
  // Start the alarm with the given plan (already resolved; see AlarmPrep). Returns false if already running.
  bool start(const AlarmPlan& plan, const AlarmSettings& settings);
  // Demo start with optional overrides ("" = use the plan). Pseudo ids are resolved locally, no network.
  bool startTest(const String& gameId, const String& tuneId);
  void update(uint32_t now);
  void onButton(uint8_t clockId);
  void abort();
  bool isRunning();
  String stateString();     // "idle" | "ringing" | "done"
  String detail();          // game id while running
  String currentTuneName(); // melody playing in this session
}
