// test_webui: joins the hotspot and serves the phone UI with FAKE alarm state so data/index.html can be
// developed without the full alarm running. Options come from the real registries (built-in tunes + any
// melodies saved on this board's LittleFS). Prints the IP on connect. Save / Test / Stop / Sync log to Serial.
// Upload the UI first:  pio run -e test_webui -t uploadfs
#include <Arduino.h>
#include "pins.h"
#include "alarm_config.h"
#include "secrets_loader.h"
#include "ring.h"
#include "melody_store.h"
#include "game_registry.h"
#include "wifi_manager.h"
#include "timekeeper.h"
#include "web_server.h"
#include "web_options.h"

// ---- fake state ----
static String alarmTime = "07:30", gameId = "random", tuneId = "new", tuneStyle = "";
static float  distanceM = 6;
static bool   ringing = false;
static uint32_t ringUntil = 0;
static uint32_t planReadyAt = 0;      // simulate "Gemini is composing..." for 6 s after save
static String planGame = "sprint", planTune = "Wasp Nest";
static String planReason = "Gemini chose Dash because your legs looked suspiciously well-rested.";

static void stateJson(JsonDocument& d) {
  uint32_t now = millis();
  bool pending = planReadyAt && now < planReadyAt;
  d["now"] = (uint32_t)time(nullptr); d["time"] = TimeKeeper::timeString(); d["date"] = TimeKeeper::dateString();
  d["timeSource"] = TimeKeeper::source(); d["uptime"] = now / 1000;
  d["isRinging"] = ringing; d["ringingGame"] = ringing ? planGame : "";
  JsonObject js = d["settings"].to<JsonObject>();
  js["alarmTime"] = alarmTime; js["enabled"] = true; js["gameId"] = gameId; js["tuneId"] = tuneId; js["distanceM"] = distanceM; js["tuneStyle"] = tuneStyle;
  // next alarm = today/tomorrow at alarmTime
  time_t t = time(nullptr); struct tm tm; localtime_r(&t, &tm);
  tm.tm_hour = alarmTime.substring(0, 2).toInt(); tm.tm_min = alarmTime.substring(3).toInt(); tm.tm_sec = 0;
  time_t a = mktime(&tm); if (a <= t) { tm.tm_mday++; a = mktime(&tm); }
  char iso[17]; strftime(iso, sizeof(iso), "%Y-%m-%dT%H:%M", &tm);
  d["nextAlarm"] = iso; d["nextAlarmEpoch"] = (uint32_t)a;
  JsonObject jp = d["plan"].to<JsonObject>();
  jp["ready"] = !pending; jp["pending"] = pending; jp["status"] = pending ? "asking Gemini..." : "ready (fake)";
  GameInfo gi; GameRegistry::info(planGame.c_str(), gi);
  jp["gameId"] = pending ? "" : planGame; jp["gameName"] = pending ? "" : gi.name;
  jp["tuneId"] = pending ? "" : "builtin:wasp_nest"; jp["tuneName"] = pending ? "" : planTune;
  jp["geminiReason"] = pending ? "" : planReason;
  d["gemini"] = "fake";
  JsonObject jw = d["wifi"].to<JsonObject>();
  jw["connected"] = WifiManager::isConnected(); jw["ip"] = WifiManager::ip(); jw["rssi"] = WifiManager::rssi(); jw["channel"] = WifiManager::channel();
  JsonObject jl = d["link"].to<JsonObject>();
  jl["ok"] = (now / 15000) % 2 == 0; jl["rtt"] = 3;      // toggles every 15 s so the dot can be seen in both states
  d["version"] = WebOptions::optionsVersion();
}

static void optionsJson(JsonDocument& d) { WebOptions::build(d); }

static String historyJson() {
  return "[{\"date\":\"2026-11-05\",\"game\":\"rotate\",\"tuneId\":\"builtin:siren_stairs\",\"tune\":\"Siren Stairs\",\"reason\":\"\",\"completed\":true},"
         "{\"date\":\"2026-11-06\",\"game\":\"sprint\",\"tuneId\":\"gen:2026-11-06_sunrise-dash\",\"tune\":\"Sunrise Dash\",\"reason\":\"Gemini chose Dash because mornings are for moving.\",\"completed\":true},"
         "{\"date\":\"2026-11-07\",\"game\":\"reaction\",\"tuneId\":\"builtin:wasp_nest\",\"tune\":\"Wasp Nest\",\"reason\":\"\",\"completed\":false}]";
}

static bool melodyJson(const String& id, JsonDocument& out) {
  Melody m; String rid;
  if (!MelodyStore::isValidTuneId(id) || MelodyStore::isPseudoTune(id)) return false;
  MelodyStore::resolve(id, m, &rid);
  deserializeJson(out, MelodyFormat::toJson(m)); out["id"] = id;
  return true;
}

static bool applySettings(JsonVariantConst in, String& err) {
  String t = in["alarmTime"] | ""; String g = in["gameId"] | ""; String tu = in["tuneId"] | "";
  if (t.length() != 5) { err = "alarmTime must be HH:MM"; return false; }
  if (!GameRegistry::exists(g.c_str()) && !GameRegistry::isPseudo(g.c_str())) { err = "unknown gameId: " + g; return false; }
  if (!MelodyStore::isValidTuneId(tu)) { err = "unknown tuneId: " + tu; return false; }
  alarmTime = t; gameId = g; tuneId = tu; distanceM = in["distanceM"] | distanceM; tuneStyle = String((const char*)(in["tuneStyle"] | tuneStyle.c_str()));
  planGame = GameRegistry::exists(g.c_str()) ? g : GameRegistry::randomName();
  planReadyAt = millis() + 6000;
  Serial.printf("[test] settings: %s game=%s tune=%s dist=%.1f (plan 'ready' in 6 s)\n", t.c_str(), g.c_str(), tu.c_str(), distanceM);
  return true;
}
static void testAlarm(const String& g, const String& tu) {
  Serial.printf("[test] TEST ALARM game=%s tune=%s -> fake ringing 8 s\n", g.c_str(), tu.c_str());
  ringing = true; ringUntil = millis() + 8000; Ring::fill(CRGB::Red); Ring::show();
}
static void stopAlarm() { Serial.println("[test] STOP"); ringing = false; Ring::off(); }
static void setTime(uint32_t epoch) { TimeKeeper::setManual(epoch); }

static void onWifi(const String& ip, uint8_t) {
  Serial.printf("[test] >>> open http://%s/ on the phone <<<\n", ip.c_str());
  WebServerUI::startMdns();
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== test_webui ===");
  Ring::begin();
  MelodyStore::begin();
  WifiManager::begin(WIFI_SSID, WIFI_PASS, "alarm-test");
  WifiManager::onConnect(onWifi);
  TimeKeeper::begin();
  WebHandlers h = { stateJson, optionsJson, historyJson, melodyJson, applySettings, testAlarm, stopAlarm, setTime };
  WebServerUI::begin(h);
}

void loop() {
  uint32_t now = millis();
  WifiManager::update(now);
  TimeKeeper::update(now);
  WebServerUI::update(now);
  if (ringing && (int32_t)(now - ringUntil) >= 0) { ringing = false; Ring::off(); }
}
