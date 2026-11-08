#include "game_rotate.h"
#include "alarm_config.h"
#include "ring.h"
#include "buzzer.h"
#include "melody.h"

namespace {
  constexpr uint16_t PAUSE_AFTER_HIT_MS = 600;
}

void GameRotate::begin() {
  _streak = 0;
  _stepMs = ROTATE_STEP_START;
  MelodyPlayer::play();
  Serial.println("[rotate] start");
  startChase(millis());
}

void GameRotate::reset() {
  _state = IDLE;
  _streak = 0;
  MelodyPlayer::stop();
  Buzzer::stopPattern();
  Ring::off();
}

bool GameRotate::isComplete() { return _state == DONE; }

bool GameRotate::onTarget() const {
  uint8_t n = Ring::count();
  return _pos == ROTATE_TARGET_IDX
      || _pos == (uint8_t)((ROTATE_TARGET_IDX + 1) % n)
      || _pos == (uint8_t)((ROTATE_TARGET_IDX + n - 1) % n);
}

void GameRotate::draw() {
  Ring::clear();
  // completed hits shown dim green on the far side of the ring so they don't hide the chaser
  for (uint8_t i = 0; i < _streak; i++) Ring::set((ROTATE_TARGET_IDX + 4 + i) % Ring::count(), CRGB(0, 40, 0));
  Ring::set(ROTATE_TARGET_IDX, CRGB::Blue);
  Ring::set(_pos, _pos == ROTATE_TARGET_IDX ? CRGB::Cyan : CRGB::White);
  Ring::show();
}

void GameRotate::startChase(uint32_t now) {
  _pos = (ROTATE_TARGET_IDX + Ring::count() / 2) % Ring::count();  // start opposite the target
  _state = CHASE;
  _nextStep = now + _stepMs;
  draw();
}

void GameRotate::update(uint32_t now) {
  switch (_state) {
    case CHASE:
      if ((int32_t)(now - _nextStep) >= 0) {
        _nextStep += _stepMs;
        _pos = (_pos + 1) % Ring::count();
        draw();
      }
      break;
    case PAUSE:
      if ((int32_t)(now - _pauseEnd) >= 0) startChase(now);
      break;
    default: break;
  }
}

void GameRotate::onButton(uint8_t clockId) {
  if (clockId != CLOCK_MASTER || _state != CHASE) return;
  if (onTarget()) {
    _streak++;
    Serial.printf("[rotate] HIT %u/%u at pos %u (step %u ms)\n", _streak, ROTATE_HITS_NEEDED, _pos, _stepMs);
    _stepMs = (_stepMs > ROTATE_STEP_FLOOR + ROTATE_STEP_DEC) ? _stepMs - ROTATE_STEP_DEC : ROTATE_STEP_FLOOR;
    Buzzer::successChirp();
    if (_streak >= ROTATE_HITS_NEEDED) {
      _state = DONE;
      Serial.println("[rotate] COMPLETE");
      MelodyPlayer::stop();
      Ring::fill(CRGB::Green); Ring::show();
    } else {
      _state = PAUSE;
      _pauseEnd = millis() + PAUSE_AFTER_HIT_MS;
      Ring::fillCount((uint8_t)((_streak * Ring::count()) / ROTATE_HITS_NEEDED), CRGB::Green); Ring::show();
    }
  } else {
    Serial.printf("[rotate] MISS at pos %u, streak %u -> 0\n", _pos, _streak);
    _streak = 0;
    _stepMs = ROTATE_STEP_START;
    Buzzer::failBurst();
    _state = PAUSE;
    _pauseEnd = millis() + PAUSE_AFTER_HIT_MS;
    Ring::fill(CRGB(60, 0, 0)); Ring::show();
  }
}
