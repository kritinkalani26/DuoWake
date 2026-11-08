#include "alarm_prep.h"
#include "alarm_scheduler.h"
#include "alarm_runner.h"
#include "alarm_config.h"
#include "game_registry.h"
#include "gemini_client.h"
#include "melody_store.h"
#include "timekeeper.h"
#include "wifi_manager.h"

namespace {
  bool     _pending = false;
  bool     _preparing = false;
  uint32_t _lastTry = 0;
  String   _status = "idle";
  constexpr uint32_t RETRY_MS = 30000;

  bool needsGemini(const AlarmSettings& s) {
    return s.game == GameRegistry::PSEUDO_RANDOM || s.game == GameRegistry::PSEUDO_SURPRISE || s.tune == "new";
  }

  // Resolve the game part of the plan. useGemini=false forces the local fallback.
  void resolveGame(const AlarmSettings& s, AlarmPlan& p, bool useGemini) {
    if (GameRegistry::exists(s.game.c_str())) { p.game = s.game; p.reason = ""; return; }
    String game, reason, err;
    if (useGemini && GeminiClient::pickGame(MelodyStore::historyJson(), GameRegistry::namesCsv(), game, reason, err)) {
      p.game = game; p.reason = reason; return;
    }
    if (useGemini) Serial.printf("[prep] Gemini game pick failed (%s) -> local random\n", err.c_str());
    // local pick avoiding yesterday's game
    String last = MelodyStore::lastGame();
    String pick = GameRegistry::randomName();
    for (uint8_t i = 0; i < 4 && pick == last && GameRegistry::count() > 1; i++) pick = GameRegistry::randomName();
    GameInfo gi; GameRegistry::info(pick.c_str(), gi);
    p.game = pick;
    p.reason = String("Gemini was unreachable, so I rolled the dice: ") + gi.name + "!";
  }

  void resolveTune(const AlarmSettings& s, AlarmPlan& p, bool useGemini) {
    bool wantNew = (s.tune == "new") || (s.game == GameRegistry::PSEUDO_SURPRISE);
    Melody m;
    if (wantNew) {
      String err;
      if (useGemini && GeminiClient::generateMelody(MelodyStore::allNames(), m, err)) {
        String id = MelodyStore::save(m, TimeKeeper::dateString());
        if (id.length()) { p.tune = id; p.melodyName = m.name; return; }
        err = "save failed";
      }
      if (useGemini) Serial.printf("[prep] Gemini melody failed (%s) -> random builtin\n", err.c_str());
      String id;
      MelodyStore::randomBuiltin(m, &id);
      p.tune = id; p.melodyName = m.name;
      p.reason += String(p.reason.length() ? " " : "") + "(No new Gemini melody this time, so it's '" + m.name + "'.)";
      return;
    }
    if (s.tune == "shuffle") {   // decided at alarm time
      p.tune = "shuffle"; p.melodyName = "Shuffle (picked at alarm time)"; return;
    }
    String id;
    MelodyStore::resolve(s.tune, m, &id);
    p.tune = id; p.melodyName = m.name;
  }

  void buildPlan(bool useGemini) {
    AlarmSettings& s = AlarmScheduler::settings();
    AlarmPlan& p = AlarmScheduler::plan();
    Serial.printf("[prep] building plan (gemini=%s) for game=%s tune=%s\n", useGemini ? "yes" : "no", s.game.c_str(), s.tune.c_str());
    AlarmPlan np;
    resolveGame(s, np, useGemini);
    resolveTune(s, np, useGemini);
    np.preparedFor = AlarmScheduler::settingsFingerprint();
    np.ready = true;
    p = np;
    AlarmScheduler::savePlan();
    _status = String("ready: ") + p.game + " / " + p.melodyName;
  }
}

void AlarmPrep::begin() {
  if (!AlarmScheduler::planIsCurrent()) requestPrep();
  else _status = "ready: " + AlarmScheduler::plan().game + " / " + AlarmScheduler::plan().melodyName;
}

void AlarmPrep::requestPrep() { _pending = true; _lastTry = 0; _status = "waiting to prepare"; }
bool AlarmPrep::isPreparing() { return _preparing || _pending; }
String AlarmPrep::status() { return _status; }

void AlarmPrep::update(uint32_t now) {
  if (!_pending || AlarmRunner::isRunning()) return;
  AlarmSettings& s = AlarmScheduler::settings();
  if (needsGemini(s) && !WifiManager::isConnected()) {
    _status = "waiting for WiFi to ask Gemini";
    if (now - _lastTry < RETRY_MS) return;
    _lastTry = now;
    Serial.println("[prep] Gemini needed but WiFi down; will retry");
    return;
  }
  _pending = false;
  _preparing = true;
  _status = needsGemini(s) ? "asking Gemini..." : "preparing";
  buildPlan(needsGemini(s));
  _preparing = false;
}

void AlarmPrep::ensurePlanNow() {
  if (AlarmScheduler::planIsCurrent()) return;
  Serial.println("[prep] plan not ready at fire time -> local fallback (no network)");
  _pending = false;
  buildPlan(false);
}
