#include "master_app.h"
#include "pins.h"
#include "alarm_config.h"
#include "secrets_loader.h"
#include "buzzer.h"
#include "button.h"
#include "ring.h"
#include "melody.h"
#include "melody_store.h"
#include "espnow_link.h"
#include "wifi_manager.h"
#include "timekeeper.h"
#include "alarm_scheduler.h"
#include "alarm_runner.h"
#include "alarm_prep.h"
#include "gemini_client.h"
#include "game_registry.h"
#include "web_server.h"
#include "web_options.h"

namespace {
  uint32_t _lastHeartbeat = 0;

  // ---------- ESP-NOW ----------
  void onLink(const LinkMsg& m) {
    if (m.type == MSG_BUTTON_PRESSED) AlarmRunner::onButton(CLOCK_SLAVE);
  }

  // ---------- WiFi ----------
  void onWifiUp(const String& ip, uint8_t ch) {
    Serial.printf("[master] >>> phone UI: http://%s/   (or http://%s.local/) <<<\n", ip.c_str(), MDNS_NAME);
    EspNowLink::lockChannel(ch);
    WebServerUI::startMdns();
  }

  // ---------- Alarm fire ----------
  void onFire() {
    AlarmPrep::ensurePlanNow();   // local fallback if Gemini prep never completed; no network here
    AlarmRunner::start(AlarmScheduler::plan(), AlarmScheduler::settings());
  }

  // ---------- Web handlers ----------
  void stateJson(JsonDocument& d) {
    const AlarmSettings& s = AlarmScheduler::settings();
    const AlarmPlan& p = AlarmScheduler::plan();
    d["now"] = (uint32_t)time(nullptr);
    d["time"] = TimeKeeper::timeString();
    d["date"] = TimeKeeper::dateString();
    d["timeSource"] = TimeKeeper::source();
    d["uptime"] = millis() / 1000;
    d["isRinging"] = AlarmRunner::isRunning();
    d["ringingGame"] = AlarmRunner::detail();
    JsonObject js = d["settings"].to<JsonObject>();
    js["alarmTime"] = AlarmScheduler::alarmTimeString();
    js["enabled"] = s.enabled;
    js["gameId"] = s.game;
    js["tuneId"] = s.tune;
    js["distanceM"] = s.distanceMeters;
    d["nextAlarm"] = AlarmScheduler::nextAlarmIso();
    d["nextAlarmEpoch"] = (uint32_t)AlarmScheduler::nextAlarmEpoch();
    JsonObject jp = d["plan"].to<JsonObject>();
    bool ready = AlarmScheduler::planIsCurrent();
    jp["ready"] = ready;
    jp["pending"] = AlarmPrep::isPreparing();
    jp["status"] = AlarmPrep::status();
    jp["gameId"] = ready ? p.game : "";
    GameInfo gi;
    jp["gameName"] = (ready && GameRegistry::info(p.game.c_str(), gi)) ? gi.name : "";
    jp["tuneId"] = ready ? p.tune : "";
    jp["tuneName"] = ready ? p.melodyName : "";
    jp["geminiReason"] = ready ? p.reason : "";
    d["gemini"] = GeminiClient::lastStatus();
    JsonObject jw = d["wifi"].to<JsonObject>();
    jw["connected"] = WifiManager::isConnected(); jw["ip"] = WifiManager::ip();
    jw["rssi"] = WifiManager::rssi(); jw["channel"] = EspNowLink::channel();
    JsonObject jl = d["link"].to<JsonObject>();
    jl["ok"] = EspNowLink::peerLinked(); jl["rtt"] = EspNowLink::lastRttMs();
    d["version"] = WebOptions::optionsVersion();
  }

  void optionsJson(JsonDocument& d) { WebOptions::build(d); }
  String historyJson() { return MelodyStore::historyJson(); }

  bool melodyJson(const String& id, JsonDocument& out) {
    if (!MelodyStore::isValidTuneId(id) || MelodyStore::isPseudoTune(id)) return false;
    Melody m;
    String rid;
    if (!MelodyStore::resolve(id, m, &rid) || rid != id) return false;
    deserializeJson(out, MelodyFormat::toJson(m));
    out["id"] = id;
    return true;
  }

  // Runs on the async task: validate, save to NVS, flag the Gemini prep for the main loop.
  bool applySettings(JsonVariantConst in, String& err) {
    AlarmSettings& s = AlarmScheduler::settings();
    String t = in["alarmTime"] | "";
    int h = -1, mi = -1;
    if (t.length() == 5 && t[2] == ':') { h = t.substring(0, 2).toInt(); mi = t.substring(3).toInt(); }
    if (h < 0 || h > 23 || mi < 0 || mi > 59) { err = "alarmTime must be HH:MM"; return false; }
    String game = in["gameId"] | "";
    if (!GameRegistry::exists(game.c_str()) && !GameRegistry::isPseudo(game.c_str())) { err = "unknown gameId: " + game; return false; }
    String tune = in["tuneId"] | "";
    if (!MelodyStore::isValidTuneId(tune)) { err = "unknown tuneId: " + tune; return false; }
    float dist = in["distanceM"] | s.distanceMeters;
    if (dist < 0.5f || dist > 200.0f) { err = "distanceM out of range"; return false; }
    s.hour = h; s.minute = mi; s.enabled = in["enabled"] | true;
    s.game = game; s.tune = tune; s.distanceMeters = dist;
    AlarmScheduler::saveSettings();
    AlarmPrep::requestPrep();
    return true;
  }

  void testAlarm(const String& gameId, const String& tuneId) { AlarmRunner::startTest(gameId, tuneId); }
  void stopAlarm() { AlarmRunner::abort(); }
  void setTime(uint32_t epoch) { TimeKeeper::setManual(epoch); }
}

void MasterApp::begin() {
  Serial.println("\n===== DuoWake : MASTER (clock A) =====");
  randomSeed(esp_random());
  Buzzer::begin(); Button::begin(); Ring::begin();
  MelodyStore::begin();

  WifiManager::begin(WIFI_SSID, WIFI_PASS, "alarm-master");
  WifiManager::onConnect(onWifiUp);
  Serial.printf("[master] MY MAC: %s\n", EspNowLink::myMac().c_str());
  EspNowLink::begin(SLAVE_MAC_BYTES);
  EspNowLink::setHandler(onLink);

  TimeKeeper::begin();
  GeminiClient::begin(GEMINI_API_KEY);
  AlarmScheduler::begin();
  AlarmScheduler::onFire(onFire);
  AlarmRunner::begin();
  AlarmPrep::begin();

  WebHandlers h = { stateJson, optionsJson, historyJson, melodyJson, applySettings, testAlarm, stopAlarm, setTime };
  WebServerUI::begin(h);
  Serial.println("[master] ready. Alarm, games and ESP-NOW work without WiFi; web UI + Gemini need the hotspot.");
}

void MasterApp::loop() {
  uint32_t now = millis();
  Button::update(now);
  Buzzer::update(now);
  MelodyPlayer::update(now);
  WifiManager::update(now);
  EspNowLink::setWifiConnected(WifiManager::isConnected());
  EspNowLink::update(now);
  TimeKeeper::update(now);
  WebServerUI::update(now);

  if (Button::pressed()) {
    if (AlarmRunner::isRunning()) AlarmRunner::onButton(CLOCK_MASTER);
    else Serial.println("[master] button (idle)");
  }

  AlarmScheduler::update(now);
  AlarmRunner::update(now);
  AlarmPrep::update(now);     // may block for a few seconds while talking to Gemini (idle only)

  if (now - _lastHeartbeat >= 10000) {
    _lastHeartbeat = now;
    Serial.printf("[master] %s %s | next %s | %s | wifi %s ip %s | slave %s | plan %s\n",
      TimeKeeper::dateString().c_str(), TimeKeeper::timeString().c_str(), AlarmScheduler::nextAlarmString().c_str(),
      AlarmRunner::stateString().c_str(), WifiManager::isConnected() ? "up" : "down", WifiManager::ip().c_str(),
      EspNowLink::peerLinked() ? "linked" : "NO LINK", AlarmPrep::status().c_str());
  }
}
