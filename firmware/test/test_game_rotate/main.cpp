// test_game_rotate: runs the ROTATE game standalone on one clock (no WiFi/ESP-NOW).
// Plays built-in tune 1 as the alarm sound. Press when the white chaser is on/next to the blue LED 0.
#include <Arduino.h>
#include "pins.h"
#include "alarm_config.h"
#include "buzzer.h"
#include "button.h"
#include "ring.h"
#include "melody.h"
#include "builtin_tunes.h"
#include "game_rotate.h"

static GameRotate game;
static uint32_t doneAt = 0;

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== test_game_rotate ===");
  Buzzer::begin(); Button::begin(); Ring::begin();
  Melody m;
  MelodyFormat::fromJson(BUILTIN_TUNES[1].json, m);
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
