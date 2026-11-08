// test_buzzer: exercises the single passive buzzer on PIN_BUZZER.
// Expect: C5..B6 scale sweep (24 notes), a fail burst, a success chirp, then each built-in tune for 4 s,
// with a fail burst injected mid-tune to prove patterns override the melody and the melody resumes.
#include <Arduino.h>
#include "pins.h"
#include "buzzer.h"
#include "melody.h"
#include "builtin_tunes.h"

enum Phase { SWEEP, BURST, CHIRP, TUNES, DONE };
static Phase phase = SWEEP;
static uint32_t phaseStart = 0;
static uint8_t step = 0;
static bool injected = false;
static Melody m;

static const char* SCALE[] = {"C5","C#5","D5","D#5","E5","F5","F#5","G5","G#5","A5","A#5","B5",
                              "C6","C#6","D6","D#6","E6","F6","F#6","G6","G#6","A6","A#6","B6"};

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== test_buzzer ===");
  Serial.printf("buzzer=GPIO%u (passive, LEDC ch %u)\n", PIN_BUZZER, BUZZER_LEDC_CHANNEL);
  Buzzer::begin();
  for (uint8_t i = 0; i < BUILTIN_TUNE_COUNT; i++) {
    String err;
    bool ok = MelodyFormat::fromJson(BUILTIN_TUNES[i].json, m, &err);
    Serial.printf("builtin[%u] %s: %s %s\n", i, BUILTIN_TUNES[i].id, ok ? "OK" : "INVALID", err.c_str());
  }
  uint16_t c5 = 0, db6 = 0, rest = 1, low = 0, bad = 0;
  MelodyFormat::noteToFreq("C5", c5);
  MelodyFormat::noteToFreq("Db6", db6);
  MelodyFormat::noteToFreq("REST", rest);
  bool lowOk = MelodyFormat::noteToFreq("A4", low);
  bool badOk = MelodyFormat::noteToFreq("H9", bad);
  Serial.printf("C5=%u Db6=%u REST=%u A4-accepted=%d H9-accepted=%d  (expect 523 1109 0 0 0)\n",
                c5, db6, rest, (int)lowOk, (int)badOk);
  phaseStart = millis();
}

void loop() {
  uint32_t now = millis();
  Buzzer::update(now);
  MelodyPlayer::update(now);

  switch (phase) {
    case SWEEP:
      if (now - phaseStart > 120) {
        phaseStart = now;
        if (step < 24) {
          uint16_t f = 0; MelodyFormat::noteToFreq(SCALE[step], f);
          Serial.printf("%s %u Hz\n", SCALE[step], f);
          Buzzer::tone(f); step++;
        } else {
          Buzzer::noTone(); step = 0; phase = BURST; Serial.println("failBurst"); Buzzer::failBurst();
        }
      }
      break;
    case BURST:
      if (!Buzzer::patternBusy() && now - phaseStart > 800) { phase = CHIRP; phaseStart = now; Serial.println("successChirp"); Buzzer::successChirp(); }
      break;
    case CHIRP:
      if (!Buzzer::patternBusy() && now - phaseStart > 800) {
        phase = TUNES; step = 0; phaseStart = now; injected = false;
        MelodyFormat::fromJson(BUILTIN_TUNES[0].json, m);
        MelodyPlayer::play(m);
      }
      break;
    case TUNES:
      if (!injected && now - phaseStart > 2000) { injected = true; Serial.println("failBurst over melody (melody should resume)"); Buzzer::failBurst(); }
      if (now - phaseStart > 4000) {
        phaseStart = now; injected = false;
        if (++step < BUILTIN_TUNE_COUNT) { MelodyFormat::fromJson(BUILTIN_TUNES[step].json, m); MelodyPlayer::play(m); }
        else { MelodyPlayer::stop(); phase = DONE; Serial.println("done. Press reset to repeat."); }
      }
      break;
    case DONE: break;
  }
}
