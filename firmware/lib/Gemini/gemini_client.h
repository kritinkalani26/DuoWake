// gemini_client.h - Gemini REST (generateContent, JSON mode) for melody generation and game picking.
// All calls are synchronous HTTPS and take a few seconds: ONLY call them when no game is running
// (e.g. right after settings are saved). Never from the alarm path.
#pragma once
#include <Arduino.h>
#include "melody.h"

namespace GeminiClient {
  void begin(const char* apiKey);

  // Ask for a brand-new 8-16 note alarm melody. existingNames = comma list so it varies.
  // style = optional free text from the user ("spooky", "8-bit boss fight"); "" = default energetic wake-up.
  // Validates; retries once on invalid output. Returns false (with err) if both attempts fail.
  bool generateMelody(const String& existingNames, const String& style, Melody& out, String& err);

  // Ask Gemini to pick one of the games. historyJson = recent history array (may be "[]").
  // gamesCsv = "reaction,rotate,sprint". Returns game + playful reason.
  bool pickGame(const String& historyJson, const String& gamesCsv, String& game, String& reason, String& err);

  // GET /v1beta/models and log the names (called automatically on 404 so we can fix the model name fast).
  void listModels();

  // Last request summary for the status panel.
  String lastStatus();
}
