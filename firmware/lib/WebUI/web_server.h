// web_server.h - MASTER's HTTP API + single-file UI from LittleFS (/index.html).
// One function per endpoint in web_server.cpp so adding one is a copy-paste.
// Handlers marked (async) run on the TCP task and must only read/copy state; those marked (loop)
// are queued and executed from WebServerUI::update() on the main loop.
//
//   GET  /                    UI
//   GET  /api/state           settings, next alarm, plan, wifi, link, uptime, isRinging
//   GET  /api/options         {games:[{id,name,tagline}], tunes:[{id,name,source,date,bars}]}
//   POST /api/settings        {alarmTime:"HH:MM", gameId, tuneId, distanceM} -> new /api/state
//   POST /api/test            {gameId?, tuneId?}  start the alarm now (demo)
//   POST /api/stop            emergency stop (demo only, logged loudly)
//   POST /api/time            {epoch}  manual clock sync
//   GET  /api/history         last 30 mornings
//   GET  /api/melody/:id      note JSON for a tune id (also /api/melody?id=...)
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

struct WebHandlers {
  void   (*stateJson)(JsonDocument& out);                          // (async)
  void   (*optionsJson)(JsonDocument& out);                        // (async)
  String (*historyJson)();                                         // (async)
  bool   (*melodyJson)(const String& id, JsonDocument& out);       // (async)
  bool   (*applySettings)(JsonVariantConst in, String& err);       // (async) validates + saves; Gemini work is deferred by the callee
  void   (*testAlarm)(const String& gameId, const String& tuneId); // (loop)
  void   (*stopAlarm)();                                           // (loop)
  void   (*setTime)(uint32_t epoch);                               // (loop)
};

namespace WebServerUI {
  bool begin(const WebHandlers& h);   // needs LittleFS mounted
  void update(uint32_t now);          // executes queued actions on the main loop
  void startMdns();                   // call after WiFi connects
}
