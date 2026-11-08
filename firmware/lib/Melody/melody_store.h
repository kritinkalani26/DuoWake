// melody_store.h - LittleFS melody library + daily history.
//   /melodies/YYYY-MM-DD_<slug>.json   one file per generated melody
//   /history.json  [{"date","game","tuneId","tune","reason","completed"}, ...]  (last 30)
//
// Tune ids (the UI only ever sends these):
//   "builtin:<slug>"            built-in tune from builtin_tunes.h
//   "gen:YYYY-MM-DD_<slug>"     Gemini melody = LittleFS filename without .json
//   "new"                       pseudo: ask Gemini for a fresh melody tonight (resolved by AlarmPrep)
//   "shuffle"                   pseudo: random saved Gemini melody at alarm time (random built-in if none)
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include "melody.h"

struct MelodyEntry {
  String id;     // gen:2026-11-08_sunrise-sprint
  String file;   // 2026-11-08_sunrise-sprint.json
  String name;   // from JSON
  String date;   // YYYY-MM-DD
};

namespace MelodyStore {
  bool begin();                                   // mounts LittleFS (formats on first failure)
  bool loadBuiltin(const char* slug, Melody& out);
  bool loadGenerated(const char* fileBase, Melody& out);   // "YYYY-MM-DD_<slug>" (no .json)
  // Resolve ANY tune id (builtin:/gen:/shuffle; "new" behaves like shuffle here) to notes.
  // Falls back to a random built-in if the id is unknown or the file is missing. resolvedId = concrete id used.
  bool resolve(const String& tuneId, Melody& out, String* resolvedId = nullptr);
  bool randomBuiltin(Melody& out, String* idOut = nullptr);
  bool randomGenerated(Melody& out, String* idOut = nullptr);   // false if none saved
  bool isPseudoTune(const String& id);                           // "new" | "shuffle"
  bool isValidTuneId(const String& id);                          // builtin:/gen: exists, or pseudo
  String tuneName(const String& id);                             // display name for any id ("" if unknown)

  // Save a melody; returns the new tune id ("gen:...") or "" on failure.
  String save(const Melody& m, const String& date);
  // List saved melodies (newest first).
  void list(std::vector<MelodyEntry>& out);
  // Names of all melodies (builtin + saved), comma separated, for the Gemini prompt.
  String allNames();
  String slugify(const String& s);
  uint32_t generation();                                         // bumps on every save (UI cache key)

  // History
  void addHistory(const String& date, const String& gameId, const String& tuneId, const String& tuneName, const String& reason);
  void markLastCompleted();      // flips "completed" on the most recent entry
  String historyJson();          // raw array json (oldest first)
  String lastGame();             // most recent game id or ""
}
