// melody.h - melody JSON format, note lookup table, and a non-blocking looping player.
// Format: {"name":"...","notes":[{"n":"C5","ms":150},{"n":"REST","ms":50},...]}
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

constexpr uint8_t MELODY_MAX_NOTES_STORED = 32;

struct Note {
  uint16_t freq;   // Hz, 0 = rest
  uint16_t ms;
};

struct Melody {
  char    name[32] = "";
  Note    notes[MELODY_MAX_NOTES_STORED];
  uint8_t count = 0;
};

namespace MelodyFormat {
  // Note name ("C5".."B6", sharps as "C#5", or "REST") -> Hz. Returns false if unknown or out of range.
  bool noteToFreq(const char* name, uint16_t& freqOut);
  // Parse + validate JSON into Melody. Error message in errOut (may be null).
  bool fromJson(const char* json, Melody& out, String* errOut = nullptr);
  bool fromJson(JsonVariantConst v, Melody& out, String* errOut = nullptr);
  // Serialize to JSON string (reconstructs note names from freq).
  String toJson(const Melody& m);
  const char* freqToName(uint16_t freq);
}

class MelodyPlayer {
public:
  static void load(const Melody& m);   // set the alarm melody without playing it
  static void play();                  // start looping the loaded melody
  static void play(const Melody& m);   // load + play
  static void stop();
  static bool isPlaying();
  static void update(uint32_t now);
  static const char* currentName();
private:
  static Melody   _m;
  static uint8_t  _idx;
  static uint32_t _noteEnd;
  static bool     _playing;
};
