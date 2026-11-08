#include "buzzer.h"
#include "pins.h"

uint16_t Buzzer::_melodyFreq = 0;
uint16_t Buzzer::_patternFreq = Buzzer::PATTERN_FREQ_HZ;
uint8_t  Buzzer::_beepsLeft = 0;
uint16_t Buzzer::_onMs = 0, Buzzer::_offMs = 0;
uint32_t Buzzer::_phaseEnd = 0;
bool     Buzzer::_phaseOn = false;
bool     Buzzer::_nag = false;

void Buzzer::begin() {
  ledcSetup(BUZZER_LEDC_CHANNEL, 2000, 10);
  ledcAttachPin(PIN_BUZZER, BUZZER_LEDC_CHANNEL);
  output(0);
}

// Raw drive of the pin. 0 = silent (pin LOW, transistor off).
void Buzzer::output(uint16_t freqHz) {
  if (freqHz == 0) { ledcWrite(BUZZER_LEDC_CHANNEL, 0); return; }
  ledcWriteTone(BUZZER_LEDC_CHANNEL, freqHz);
  ledcWrite(BUZZER_LEDC_CHANNEL, 512);  // 50% duty on 10-bit
}

// ---- melody layer ----
void Buzzer::tone(uint16_t freqHz) {
  _melodyFreq = freqHz;
  if (!patternBusy()) output(_melodyFreq);
}
void Buzzer::noTone() { tone(0); }

// ---- pattern layer ----
void Buzzer::beepPattern(uint8_t count, uint16_t onMs, uint16_t offMs, uint16_t freqHz) {
  _nag = false;
  _beepsLeft = count;
  _onMs = onMs; _offMs = offMs;
  _patternFreq = freqHz;
  _phaseOn = true;
  _phaseEnd = millis() + onMs;
  output(_patternFreq);
}

void Buzzer::failBurst()    { beepPattern(3, 80, 70, PATTERN_FREQ_HZ); }
void Buzzer::successChirp() { beepPattern(1, 60, 0, CHIRP_FREQ_HZ); }

void Buzzer::startNag() {
  _nag = true;
  _beepsLeft = 1;
  _onMs = 250; _offMs = 250;
  _patternFreq = PATTERN_FREQ_HZ;
  _phaseOn = true;
  _phaseEnd = millis() + _onMs;
  output(_patternFreq);
}

void Buzzer::stopPattern() {
  _nag = false;
  _beepsLeft = 0;
  output(_melodyFreq);   // hand the pin back to the melody
}

bool Buzzer::patternBusy() { return _beepsLeft > 0 || _nag; }

void Buzzer::update(uint32_t now) {
  if (!patternBusy()) return;
  if ((int32_t)(now - _phaseEnd) < 0) return;
  if (_phaseOn) {
    _phaseOn = false;
    if (!_nag) _beepsLeft--;
    if (!patternBusy()) { output(_melodyFreq); return; }   // pattern finished -> restore melody
    output(0);
    _phaseEnd = now + _offMs;
  } else {
    _phaseOn = true;
    output(_patternFreq);
    _phaseEnd = now + _onMs;
  }
}
