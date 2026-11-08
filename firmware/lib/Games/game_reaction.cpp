#include "game_reaction.h"
#include "alarm_config.h"
#include "ring.h"
#include "buzzer.h"
#include "melody.h"

void GameReaction::begin() {
  _streak = 0;
  _windowMs = REACTION_WINDOW_START;
  MelodyPlayer::play();
  Serial.println("[reaction] start");
  showStreak();
  scheduleNext(millis());
}

void GameReaction::reset() {
  _state = IDLE;
  _streak = 0;
  MelodyPlayer::stop();
  Buzzer::stopPattern();
  Ring::off();
}

bool GameReaction::isComplete() { return _state == DONE; }

void GameReaction::showStreak() {
  Ring::fillCount((uint8_t)((_streak * Ring::count()) / REACTION_HITS_NEEDED), CRGB::Green);
  Ring::show();
}

void GameReaction::scheduleNext(uint32_t now) {
  _state = GAP;
  _deadline = now + random(REACTION_GAP_MIN_MS, REACTION_GAP_MAX_MS + 1);
}

void GameReaction::miss(const char* why) {
  Serial.printf("[reaction] MISS (%s) streak %u -> 0\n", why, _streak);
  _streak = 0;
  _windowMs = REACTION_WINDOW_START;
  Buzzer::failBurst();
  showStreak();
  scheduleNext(millis());
}

void GameReaction::update(uint32_t now) {
  switch (_state) {
    case GAP:
      if ((int32_t)(now - _deadline) >= 0) {
        _litIdx = random(Ring::count());
        Ring::clear();
        Ring::set(_litIdx, CRGB::White);
        Ring::show();
        _state = LIT;
        _deadline = now + _windowMs;
        Serial.printf("[reaction] LED %u lit, window %u ms\n", _litIdx, _windowMs);
      }
      break;
    case LIT:
      if ((int32_t)(now - _deadline) >= 0) miss("too slow");
      break;
    default: break;
  }
}

void GameReaction::onButton(uint8_t clockId) {
  if (clockId != CLOCK_MASTER) return;
  if (_state == LIT) {
    _streak++;
    Serial.printf("[reaction] HIT %u/%u (reacted in %ld ms)\n", _streak, REACTION_HITS_NEEDED,
                  (long)(_windowMs - (int32_t)(_deadline - millis())));
    if (_windowMs > REACTION_WINDOW_FLOOR + REACTION_WINDOW_SHRINK) _windowMs -= REACTION_WINDOW_SHRINK;
    else _windowMs = REACTION_WINDOW_FLOOR;
    Buzzer::successChirp();
    showStreak();
    if (_streak >= REACTION_HITS_NEEDED) {
      _state = DONE;
      Serial.println("[reaction] COMPLETE");
      MelodyPlayer::stop();
      Ring::fill(CRGB::Green); Ring::show();
    } else {
      scheduleNext(millis());
    }
  } else if (_state == GAP) {
    miss("pressed with nothing lit");
  }
}
