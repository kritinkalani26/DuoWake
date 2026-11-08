#include "game_sprint.h"
#include "alarm_config.h"
#include "ring.h"
#include "buzzer.h"
#include "melody.h"
#include "espnow_link.h"

namespace {
  constexpr uint16_t FAIL_PAUSE_MS   = 1500;
  constexpr uint16_t SLAVE_RESEND_MS = 500;   // ESP-NOW has no retry; keep repeating the slave's current order
}

void GameSprint::configure(const GameParams& p) {
  _distance = p.distanceMeters > 0.5f ? p.distanceMeters : 0.5f;
  _allowedMs = (uint32_t)((_distance / RUN_PACE_MPS + LEG_SLACK_S) * 1000.0f);
}

uint8_t GameSprint::progressLeds() const {
  return (uint8_t)((_legsDone * Ring::count()) / SPRINT_LEGS);
}

// Tell the slave what it should be doing right now. Repeated from update() so a lost packet
// cannot leave the slave ringing forever or never ringing at all (both looked like "it never ends").
void GameSprint::commandSlave(MsgType cmd) {
  _slaveCmd = cmd;
  _slaveResendAt = millis() + SLAVE_RESEND_MS;
  EspNowLink::send(cmd);
  EspNowLink::send(MSG_SET_LEDS, progressLeds());
}

void GameSprint::ringMaster(bool on) {
  if (on) {
    MelodyPlayer::play();
    // red = "press me", with the legs already done kept green so progress is visible while ringing
    Ring::fill(CRGB::Red);
    for (uint8_t i = 0; i < progressLeds(); i++) Ring::set(i, CRGB::Green);
    Ring::show();
  } else {
    MelodyPlayer::stop();
    Buzzer::stopPattern();
    showProgress();
  }
}

void GameSprint::showProgress() {
  Ring::fillCount(progressLeds(), CRGB::Green);
  Ring::show();
  EspNowLink::send(MSG_SET_LEDS, progressLeds());
}

void GameSprint::begin() {
  GameParams p; p.distanceMeters = _distance;
  configure(p);   // recompute allowed time in case constants changed
  _legsDone = 0;
  Serial.printf("[sprint] start: %u legs, distance %.1f m, allowed %lu ms/leg\n",
                SPRINT_LEGS, _distance, (unsigned long)_allowedMs);
  Serial.printf("[sprint] sequence: press A, run, press B, run, press A ... %u presses in total, then done\n", SPRINT_LEGS + 1);
  commandSlave(MSG_STOP);
  _state = RING_A;
  ringMaster(true);
  Serial.println("[sprint] ring MASTER: press A to start the clock");
}

void GameSprint::reset() {
  _state = IDLE;
  _legsDone = 0;
  MelodyPlayer::stop();
  Buzzer::stopPattern();
  Ring::off();
  EspNowLink::send(MSG_STOP);
  EspNowLink::send(MSG_SET_LEDS, 0);
}

bool GameSprint::isComplete() { return _state == DONE; }

bool GameSprint::legOk(uint32_t now, uint32_t& legMs) {
  legMs = now - _legStart;
  return legMs <= _allowedMs;
}

void GameSprint::fail(uint32_t legMs) {
  Serial.printf("[sprint] LEG FAIL: %lu ms > %lu ms allowed. Restarting from leg 1 (progress reset).\n",
                (unsigned long)legMs, (unsigned long)_allowedMs);
  _legsDone = 0;
  Buzzer::failBurst();
  EspNowLink::send(MSG_FAIL_BEEP);
  commandSlave(MSG_STOP);
  Ring::fill(CRGB(80, 0, 0)); Ring::show();
  _state = FAIL_PAUSE;
  _pauseEnd = millis() + FAIL_PAUSE_MS;
}

void GameSprint::complete() {
  _state = DONE;
  _legsDone = SPRINT_LEGS;
  MelodyPlayer::stop();
  Buzzer::stopPattern();
  Serial.printf("[sprint] COMPLETE: all %u legs done\n", SPRINT_LEGS);
  Ring::fill(CRGB::Green); Ring::show();
  commandSlave(MSG_STOP);
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
      if ((int32_t)(now - _pauseEnd) >= 0) { _state = RING_A; ringMaster(true); Serial.println("[sprint] leg 1: ring MASTER, press A to start again"); }
      break;
    default: break;
  }
  // Keep the slave in sync while the game is live (RING while it should ring, STOP otherwise).
  if (_state != IDLE && _state != DONE && (int32_t)(now - _slaveResendAt) >= 0) {
    _slaveResendAt = now + SLAVE_RESEND_MS;
    EspNowLink::send(_slaveCmd);
    EspNowLink::send(MSG_SET_LEDS, progressLeds());
  }
}

void GameSprint::onButton(uint8_t clockId) {
  uint32_t now = millis();
  uint32_t legMs = 0;
  switch (_state) {
    case RING_A:
      if (clockId != CLOCK_MASTER) return;
      ringMaster(false);
      _legStart = now;
      _state = RUN_TO_B;
      commandSlave(MSG_RING);
      Serial.printf("[sprint] leg %u/%u: MASTER pressed, SLAVE ringing, run to B!\n", _legsDone + 1, SPRINT_LEGS);
      break;

    case RUN_TO_B:
      if (clockId != CLOCK_SLAVE) { Serial.println("[sprint] ignored MASTER press (run to SLAVE)"); return; }
      if (!legOk(now, legMs)) { fail(legMs); return; }
      _legsDone++;
      Serial.printf("[sprint] leg %u/%u OK in %lu ms (%lu allowed)\n", _legsDone, SPRINT_LEGS, (unsigned long)legMs, (unsigned long)_allowedMs);
      Buzzer::successChirp();
      if (_legsDone >= SPRINT_LEGS) { complete(); return; }
      // SLAVE silenced itself on press; now MASTER rings for the run back.
      _legStart = now;
      _state = RUN_TO_A;
      commandSlave(MSG_STOP);
      ringMaster(true);
      Serial.printf("[sprint] leg %u/%u: MASTER ringing, run back to A!\n", _legsDone + 1, SPRINT_LEGS);
      break;

    case RUN_TO_A:
      if (clockId != CLOCK_MASTER) { Serial.println("[sprint] ignored SLAVE press (run to MASTER)"); return; }
      if (!legOk(now, legMs)) { ringMaster(false); fail(legMs); return; }
      _legsDone++;
      Serial.printf("[sprint] leg %u/%u OK in %lu ms (%lu allowed)\n", _legsDone, SPRINT_LEGS, (unsigned long)legMs, (unsigned long)_allowedMs);
      ringMaster(false);
      Buzzer::successChirp();
      if (_legsDone >= SPRINT_LEGS) { complete(); return; }
      _legStart = now;
      _state = RUN_TO_B;
      commandSlave(MSG_RING);
      Serial.printf("[sprint] leg %u/%u: SLAVE ringing, run to B!\n", _legsDone + 1, SPRINT_LEGS);
      break;

    default: break;
  }
}
