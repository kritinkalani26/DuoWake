// web_options.h - builds the /api/options document from the registries so the UI never hardcodes names.
//   games: real games from GameRegistry + the two pseudo options (random, surprise)
//   tunes: pseudo (new, shuffle) + saved Gemini melodies (newest first) + built-ins,
//          each with a 12-value "bars" array (0..1 normalised pitch) for the little waveform.
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "melody.h"

namespace WebOptions {
  void build(JsonDocument& out);
  // 12 bars from a melody's pitches (0 = rest/lowest, 1 = highest in C5..B6).
  void bars(const Melody& m, JsonArray out);
  // Changes whenever a melody is saved so the UI knows to refetch options.
  uint32_t optionsVersion();
}
