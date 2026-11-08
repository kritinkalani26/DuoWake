// test_game_reaction: runs the REACTION game standalone on one clock (no WiFi/ESP-NOW).
// Plays built-in tune 0 as the alarm sound. Watch Serial for HIT/MISS; game restarts 3 s after completion.
#include <Arduino.h>
#include "pins.h"
#include "alarm_config.h"
#include "buzzer.h"
#include "button.h"
#include "ring.h"
#include "melody.h"
#include "builtin_tunes.h"
#include "game_reaction.h"

static GameReaction game;
static uint32_t doneAt = 0;

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== test_game_reaction ===");
  Buzzer::begin(); Button::begin(); Ring::begin();
  Melody m;
  MelodyFormat::fromJson(BUILTIN_TUNES[0].json, m);
  MelodyPlayer::load(m);
  randomSeed(esp_random());
  game.begin();
}

void loop() {
  uint32_t now = millis();
  Button::update(now);
  Buzzer::update(now);
  MelodyPlayer::update(now);
  if (Button::pressed()) game.onButton(CLOCK_MASTER);
  game.update(now);
  if (game.isComplete()) {
    if (!doneAt) { doneAt = now; Serial.println("complete -> restart in 3 s"); }
    else if (now - doneAt > 3000) { doneAt = 0; game.reset(); game.begin(); }
  }
}
