// test_gemini: joins the hotspot, then (1) asks Gemini to pick a game, (2) asks for a new melody,
// validates it, saves it to LittleFS and plays it on the passive buzzer. Press the button to run again.
#include <Arduino.h>
#include "pins.h"
#include "alarm_config.h"
#include "buzzer.h"
#include "button.h"
#include "ring.h"
#include "melody.h"
#include "melody_store.h"
#include "wifi_manager.h"
#include "gemini_client.h"
#include "timekeeper.h"
#include "secrets_loader.h"

static bool ranOnce = false;

static void runGemini() {
  Ring::fill(CRGB::Yellow); Ring::show();
  Serial.println("\n--- game pick ---");
  String game, reason, err;
  if (GeminiClient::pickGame(MelodyStore::historyJson(), "reaction,rotate,sprint", game, reason, err))
    Serial.printf("PICK OK: %s - %s\n", game.c_str(), reason.c_str());
  else
    Serial.printf("PICK FAILED: %s\n", err.c_str());

  Serial.println("\n--- melody ---");
  Melody m;
  String names = MelodyStore::allNames();
  Serial.printf("existing: %s\n", names.c_str());
  if (GeminiClient::generateMelody(names, "", m, err)) {
    Serial.printf("MELODY OK: %s\n", MelodyFormat::toJson(m).c_str());
    String id = MelodyStore::save(m, TimeKeeper::dateString());
    Serial.printf("saved as %s\n", id.c_str());
    MelodyStore::addHistory(TimeKeeper::dateString(), game.length() ? game : "reaction", id, m.name, reason);
    MelodyPlayer::play(m);
    Ring::fill(CRGB::Green); Ring::show();
  } else {
    Serial.printf("MELODY FAILED: %s -> falling back to builtin\n", err.c_str());
    MelodyStore::randomBuiltin(m);
    MelodyPlayer::play(m);
    Ring::fill(CRGB::Red); Ring::show();
  }
  Serial.println("\nsaved melodies:");
  std::vector<MelodyEntry> list; MelodyStore::list(list);
  for (auto& e : list) Serial.printf("  %s  %s  (%s)\n", e.date.c_str(), e.name.c_str(), e.id.c_str());
  Serial.printf("history: %s\n", MelodyStore::historyJson().c_str());
  Serial.println("press button to run again, melody plays for 6 s");
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== test_gemini ===");
  Buzzer::begin(); Button::begin(); Ring::begin();
  MelodyStore::begin();
  WifiManager::begin(WIFI_SSID, WIFI_PASS, "alarm-test");
  TimeKeeper::begin();
  GeminiClient::begin(GEMINI_API_KEY);
}

void loop() {
  uint32_t now = millis();
  static uint32_t playUntil = 0;
  Button::update(now); Buzzer::update(now); MelodyPlayer::update(now);
  WifiManager::update(now); TimeKeeper::update(now);
  if (MelodyPlayer::isPlaying() && playUntil && (int32_t)(now - playUntil) >= 0) { MelodyPlayer::stop(); playUntil = 0; }

  bool go = false;
  if (!ranOnce && WifiManager::isConnected() && now > 3000) { ranOnce = true; go = true; }
  if (Button::pressed()) go = true;
  if (go) {
    if (!WifiManager::isConnected()) { Serial.println("no WiFi yet"); return; }
    runGemini();
    playUntil = millis() + 6000;
  }
}
