#include "alarm_runner.h"
#include "alarm_config.h"
#include "game_registry.h"
#include "melody.h"
#include "melody_store.h"
#include "buzzer.h"
#include "ring.h"
#include "espnow_link.h"
#include "timekeeper.h"

namespace {
  enum State { IDLE, RUNNING, DONE };
  State    _state = IDLE;
  Game*    _game = nullptr;
  String   _gameId;
  String   _tuneName;
  uint32_t _doneAt = 0;
  uint32_t _lastStopResend = 0;
  constexpr uint32_t CELEBRATE_MS = 4000;

  void destroyGame() {
    if (_game) { delete _game; _game = nullptr; }
  }
}

void AlarmRunner::begin() {}

bool AlarmRunner::start(const AlarmPlan& plan, const AlarmSettings& settings) {
  if (_state == RUNNING) { Serial.println("[runner] already running"); return false; }
  destroyGame();

  // 1. melody ("shuffle" is resolved right here, at alarm time)
  Melody m;
  String tuneId;
  MelodyStore::resolve(plan.tune, m, &tuneId);
  MelodyPlayer::load(m);
  EspNowLink::sendMelody(m);
  _tuneName = m.name;

  // 2. game
  String gid = plan.game;
  if (!GameRegistry::exists(gid.c_str())) {
    gid = GameRegistry::randomName();
    Serial.printf("[runner] plan game '%s' unknown -> '%s'\n", plan.game.c_str(), gid.c_str());
  }
  _game = GameRegistry::create(gid.c_str());
  GameParams p; p.distanceMeters = settings.distanceMeters;
  _game->configure(p);
  _gameId = gid;

  Serial.printf("[runner] ===== ALARM START: game=%s melody='%s' (%s) =====\n", gid.c_str(), m.name, tuneId.c_str());
  if (plan.reason.length()) Serial.printf("[runner] Gemini's reason: %s\n", plan.reason.c_str());
  MelodyStore::addHistory(TimeKeeper::dateString(), gid, tuneId, m.name, plan.reason);

  _state = RUNNING;
  _game->begin();
  return true;
}

bool AlarmRunner::startTest(const String& gameId, const String& tuneId) {
  // Start from tomorrow's prepared plan. Concrete ids from the phone override it; pseudo ids
  // ("new", "random", "surprise") keep what Gemini already prepared so the demo plays the melody
  // the user just watched being composed, and only fall back to a local pick if no plan is ready.
  AlarmPlan p = AlarmScheduler::plan();
  bool planReady = AlarmScheduler::planIsCurrent();
  if (gameId.length()) {
    if (GameRegistry::exists(gameId.c_str())) { p.game = gameId; p.reason = "Demo run from the phone."; }
    else if (GameRegistry::isPseudo(gameId.c_str())) {
      if (planReady && GameRegistry::exists(p.game.c_str())) p.reason = "Demo run of tomorrow's plan.";
      else { p.game = GameRegistry::randomName(); p.reason = "Demo run: picked locally, no Gemini call."; }
    }
  }
  if (tuneId.length()) {
    if (tuneId == "new") {
      // Gemini can't be awaited in a demo: use the melody it already composed for this plan, else shuffle
      if (!(planReady && p.tune.startsWith("gen:"))) p.tune = "shuffle";
    } else p.tune = tuneId;
  }
  if (!p.game.length()) p.game = GameRegistry::randomName();
  if (!p.tune.length()) p.tune = "shuffle";
  Serial.printf("[runner] TEST start game=%s tune=%s\n", p.game.c_str(), p.tune.c_str());
  return start(p, AlarmScheduler::settings());
}

void AlarmRunner::update(uint32_t now) {
  switch (_state) {
    case RUNNING:
      if (!_game) { _state = IDLE; return; }
      _game->update(now);
      if (_game->isComplete()) {
        Serial.println("[runner] ===== ALARM COMPLETE - good morning =====");
        MelodyPlayer::stop();
        Buzzer::stopPattern();
        EspNowLink::send(MSG_STOP);
        EspNowLink::send(MSG_SET_LEDS, Ring::count());
        MelodyStore::markLastCompleted();
        _state = DONE;
        _doneAt = now;
      }
      break;
    case DONE:
      Ring::breathe(now, CRGB::Green);
      // ESP-NOW has no retry: repeat STOP once a second during the celebration so a lost packet
      // can never leave the slave ringing after the game is over.
      if (now - _lastStopResend >= 1000) { _lastStopResend = now; EspNowLink::send(MSG_STOP); EspNowLink::send(MSG_SET_LEDS, Ring::count()); }
      if (now - _doneAt > CELEBRATE_MS) {
        destroyGame();
        Ring::off();
        EspNowLink::send(MSG_STOP);
        EspNowLink::send(MSG_SET_LEDS, 0);
        _state = IDLE;
        Serial.println("[runner] idle");
      }
      break;
    default: break;
  }
}

void AlarmRunner::onButton(uint8_t clockId) {
  if (_state == RUNNING && _game) _game->onButton(clockId);
}

void AlarmRunner::abort() {
  if (_state == IDLE) return;
  Serial.println("[runner] !!!!! EMERGENCY STOP from the phone - alarm aborted without completing the game !!!!!");
  if (_game) _game->reset();
  destroyGame();
  MelodyPlayer::stop();
  Buzzer::stopPattern();
  Ring::off();
  EspNowLink::send(MSG_STOP);
  EspNowLink::send(MSG_SET_LEDS, 0);
  _state = IDLE;
}

bool AlarmRunner::isRunning() { return _state == RUNNING; }
String AlarmRunner::stateString() { return _state == RUNNING ? "ringing" : _state == DONE ? "done" : "idle"; }
String AlarmRunner::detail() { return _state == RUNNING ? _gameId : String(""); }
String AlarmRunner::currentTuneName() { return _tuneName; }
