#include "game_sprint.h"
#include "alarm_config.h"
#include "ring.h"
#include "buzzer.h"
#include "melody.h"
#include "espnow_link.h"

namespace {
  constexpr uint16_t FAIL_PAUSE_MS = 1500;
}

void GameSprint::configure(const GameParams& p) {
  _distance = p.distanceMeters > 0.5f ? p.distanceMeters : 0.5f;
  _allowedMs = (uint32_t)((_distance / RUN_PACE_MPS + LEG_SLACK_S) * 1000.0f);
}

void GameSprint::ringMaster(bool on) {
  if (on) {
    MelodyPlayer::play();
    Ring::fill(CRGB::Red); Ring::show();
  } else {
    MelodyPlayer::stop();
    Buzzer::stopPattern();
    showProgress();
  }
}

void GameSprint::showProgress() {
  uint8_t per = Ring::count() / SPRINT_LEGS;
  Ring::fillCount(_legsDone * per, CRGB::Green);
  Ring::show();
  EspNowLink::send(MSG_SET_LEDS, _legsDone * per);
}

void GameSprint::begin() {
  GameParams p; p.distanceMeters = _distance;
  configure(p);   // recompute allowed time in case constants changed
  _legsDone = 0;
  Serial.printf("[sprint] start: %u legs, distance %.1f m, allowed %lu ms/leg\n",
                SPRINT_LEGS, _distance, (unsigned long)_allowedMs);
  EspNowLink::send(MSG_STOP);
  EspNowLink::send(MSG_SET_LEDS, 0);
  _state = RING_A;
  ringMaster(true);
}

void GameSprint::reset() {
  _state = IDLE;
  _legsDone = 0;
  MelodyPlayer::stop();
  Buzzer::stopPattern();
  Ring::off();
  EspNowLink::send(MSG_STOP);
}

bool GameSprint::isComplete() { return _state == DONE; }

bool GameSprint::legOk(uint32_t now, uint32_t& legMs) {
  legMs = now - _legStart;
  return legMs <= _allowedMs;
}

void GameSprint::fail(uint32_t legMs) {
  Serial.printf("[sprint] LEG FAIL: %lu ms > %lu ms allowed. Restarting from leg 1.\n",
                (unsigned long)legMs, (unsigned long)_allowedMs);
  _legsDone = 0;
  Buzzer::failBurst();
  EspNowLink::send(MSG_FAIL_BEEP);
  EspNowLink::send(MSG_STOP);
  EspNowLink::send(MSG_SET_LEDS, 0);
  Ring::fill(CRGB(80, 0, 0)); Ring::show();
  _state = FAIL_PAUSE;
  _pauseEnd = millis() + FAIL_PAUSE_MS;
}

void GameSprint::update(uint32_t now) {
  switch (_state) {
    case RUN_TO_B:
    case RUN_TO_A: {
      // Live timeout: don't wait for a late press to declare the leg failed.
      uint32_t legMs;
      if (!legOk(now, legMs)) {
        if (_state == RUN_TO_A) ringMaster(false);
        fail(legMs);
      }
      break;
    }
    case FAIL_PAUSE:
      if ((int32_t)(now - _pauseEnd) >= 0) { _state = RING_A; ringMaster(true); Serial.println("[sprint] leg 1: ring MASTER"); }
      break;
    default: break;
  }
}

void GameSprint::onButton(uint8_t clockId) {
  uint32_t now = millis();
  uint32_t legMs = 0;
  switch (_state) {
    case RING_A:
      if (clockId != CLOCK_MASTER) return;
      ringMaster(false);
      EspNowLink::send(MSG_RING);
      _legStart = now;
      _state = RUN_TO_B;
      Serial.printf("[sprint] leg %u: MASTER pressed, SLAVE ringing, run!\n", _legsDone + 1);
      break;

    case RUN_TO_B:
      if (clockId != CLOCK_SLAVE) { Serial.println("[sprint] ignored MASTER press (run to SLAVE)"); return; }
      if (!legOk(now, legMs)) { fail(legMs); return; }
      _legsDone++;
      Serial.printf("[sprint] leg %u OK in %lu ms (%lu allowed)\n", _legsDone, (unsigned long)legMs, (unsigned long)_allowedMs);
      Buzzer::successChirp();
      showProgress();
      if (_legsDone >= SPRINT_LEGS) { _state = DONE; Serial.println("[sprint] COMPLETE"); Ring::fill(CRGB::Green); Ring::show(); EspNowLink::send(MSG_STOP); return; }
      // SLAVE silenced itself on press; now MASTER rings for the run back.
      _legStart = now;
      _state = RUN_TO_A;
      ringMaster(true);
      Serial.printf("[sprint] leg %u: MASTER ringing, run back!\n", _legsDone + 1);
      break;

    case RUN_TO_A:
      if (clockId != CLOCK_MASTER) { Serial.println("[sprint] ignored SLAVE press (run to MASTER)"); return; }
      if (!legOk(now, legMs)) { ringMaster(false); fail(legMs); return; }
      _legsDone++;
      Serial.printf("[sprint] leg %u OK in %lu ms (%lu allowed)\n", _legsDone, (unsigned long)legMs, (unsigned long)_allowedMs);
      ringMaster(false);
      Buzzer::successChirp();
      if (_legsDone >= SPRINT_LEGS) { _state = DONE; Serial.println("[sprint] COMPLETE"); Ring::fill(CRGB::Green); Ring::show(); EspNowLink::send(MSG_STOP); return; }
      EspNowLink::send(MSG_RING);
      _legStart = now;
      _state = RUN_TO_B;
      Serial.printf("[sprint] leg %u: SLAVE ringing, run!\n", _legsDone + 1);
      break;

    default: break;
  }
}
