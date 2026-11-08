// buzzer.h - the ONE passive buzzer per clock (PIN_BUZZER, LEDC PWM).
// Two layers share it:
//   - melody tone:  tone(freq) / noTone()   (set by MelodyPlayer)
//   - patterns:     failBurst(), successChirp(), beepPattern(), startNag()
// A running pattern takes priority; when it ends the melody tone is restored automatically.
// Fully non-blocking: call Buzzer::update(now) every loop().
#pragma once
#include <Arduino.h>

class Buzzer {
public:
  static void begin();
  static void update(uint32_t now);

  // --- melody layer ---
  static void tone(uint16_t freqHz);          // 0 = silence
  static void noTone();

  // --- pattern layer (overrides the melody while running) ---
  static void beepPattern(uint8_t count, uint16_t onMs, uint16_t offMs, uint16_t freqHz = PATTERN_FREQ_HZ);
  static void failBurst();     // 3 fast harsh beeps
  static void successChirp();  // 1 short high chirp
  static void startNag();      // endless on/off nag until stopPattern() (sprint ring)
  static void stopPattern();
  static bool patternBusy();

  static constexpr uint16_t PATTERN_FREQ_HZ = 2400;   // harsh, near piezo resonance
  static constexpr uint16_t CHIRP_FREQ_HZ   = 3200;

private:
  static void output(uint16_t freqHz);
  static uint16_t _melodyFreq;
  static uint16_t _patternFreq;
  static uint8_t  _beepsLeft;
  static uint16_t _onMs, _offMs;
  static uint32_t _phaseEnd;
  static bool     _phaseOn;
  static bool     _nag;
};
