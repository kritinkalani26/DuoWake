#include "web_server.h"
#include "alarm_config.h"
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>

namespace {
  AsyncWebServer _server(80);
  WebHandlers _h;
  bool _mdnsStarted = false;

  // ---- actions queued by async handlers, executed in update() on the main loop ----
  volatile bool _doTest = false, _doStop = false, _doSetTime = false;
  uint32_t _pendingEpoch = 0;
  String   _pendingTestGame, _pendingTestTune;

  // ---- helpers ----
  void sendJson(AsyncWebServerRequest* req, JsonDocument& doc, int code = 200) {
    AsyncResponseStream* res = req->beginResponseStream("application/json");
    res->setCode(code);
    serializeJson(doc, *res);
    req->send(res);
  }
  void sendOk(AsyncWebServerRequest* req, const char* msg) {
    JsonDocument d; d["ok"] = true; d["msg"] = msg; sendJson(req, d);
  }
  void sendErr(AsyncWebServerRequest* req, const String& msg, int code = 400) {
    JsonDocument d; d["ok"] = false; d["error"] = msg; sendJson(req, d, code);
  }

  // ======================= endpoints (one function each) =======================

  void handleRoot(AsyncWebServerRequest* req) {
    if (LittleFS.exists("/index.html")) req->send(LittleFS, "/index.html", "text/html");
    else req->send(500, "text/plain", "index.html missing: run  pio run -e master -t uploadfs");
  }

  void handleState(AsyncWebServerRequest* req) {
    JsonDocument d;
    if (_h.stateJson) _h.stateJson(d);
    sendJson(req, d);
  }

  void handleOptions(AsyncWebServerRequest* req) {
    JsonDocument d;
    if (_h.optionsJson) _h.optionsJson(d);
    sendJson(req, d);
  }

  void handleHistory(AsyncWebServerRequest* req) {
    req->send(200, "application/json", _h.historyJson ? _h.historyJson() : String("[]"));
  }

  void handleMelody(AsyncWebServerRequest* req) {
    String id;
    if (req->hasParam("id")) id = req->getParam("id")->value();
    else {
      String url = req->url();                       // /api/melody/<id>  (url() is already URL-decoded)
      int i = url.indexOf("/api/melody/");
      if (i >= 0) id = url.substring(i + 12);
    }
    if (!id.length()) { sendErr(req, "missing id"); return; }
    JsonDocument d;
    if (_h.melodyJson && _h.melodyJson(id, d)) sendJson(req, d);
    else sendErr(req, "unknown tune id: " + id, 404);
  }

  void handleSettings(AsyncWebServerRequest* req, JsonVariant& json) {
    String err;
    if (!_h.applySettings || !_h.applySettings(json.as<JsonVariantConst>(), err)) { sendErr(req, err.length() ? err : "rejected"); return; }
    JsonDocument d;                                   // respond with the fresh state
    if (_h.stateJson) _h.stateJson(d);
    d["ok"] = true;
    sendJson(req, d);
  }

  void handleTest(AsyncWebServerRequest* req, JsonVariant& json) {
    _pendingTestGame = String((const char*)(json["gameId"] | ""));
    _pendingTestTune = String((const char*)(json["tuneId"] | ""));
    _doTest = true;
    sendOk(req, "alarm starting");
  }

  void handleStop(AsyncWebServerRequest* req) {
    _doStop = true;
    sendOk(req, "stopping");
  }

  void handleTime(AsyncWebServerRequest* req, JsonVariant& json) {
    uint32_t epoch = json["epoch"] | 0UL;
    if (epoch < 1700000000UL) { sendErr(req, "bad epoch"); return; }
    _pendingEpoch = epoch; _doSetTime = true;
    sendOk(req, "time set");
  }

  void handleNotFound(AsyncWebServerRequest* req) {
    if (req->url().startsWith("/api/melody/")) { handleMelody(req); return; }
    req->send(404, "text/plain", "not found");
  }
}

bool WebServerUI::begin(const WebHandlers& h) {
  _h = h;
  _server.on("/",            HTTP_GET,  handleRoot);
  _server.on("/api/state",   HTTP_GET,  handleState);
  _server.on("/api/options", HTTP_GET,  handleOptions);
  _server.on("/api/history", HTTP_GET,  handleHistory);
  _server.on("/api/melody",  HTTP_GET,  handleMelody);       // ?id=...
  _server.on("/api/stop",    HTTP_POST, handleStop);
  // JSON-body endpoints (client must send Content-Type: application/json; "{}" is fine for /api/test)
  _server.addHandler(new AsyncCallbackJsonWebHandler("/api/settings", handleSettings));
  _server.addHandler(new AsyncCallbackJsonWebHandler("/api/test",     handleTest));
  _server.addHandler(new AsyncCallbackJsonWebHandler("/api/time",     handleTime));
  _server.onNotFound(handleNotFound);                          // /api/melody/<id>
  _server.begin();
  Serial.println("[web] HTTP server started on port 80");
  return true;
}

void WebServerUI::startMdns() {
  if (_mdnsStarted) MDNS.end();
  _mdnsStarted = MDNS.begin(MDNS_NAME);
  if (_mdnsStarted) { MDNS.addService("http", "tcp", 80); Serial.printf("[web] mDNS: http://%s.local/ (may not work on Android hotspots)\n", MDNS_NAME); }
  else Serial.println("[web] mDNS failed to start");
}

void WebServerUI::update(uint32_t now) {
  (void)now;
  if (_doSetTime) { _doSetTime = false; if (_h.setTime) _h.setTime(_pendingEpoch); }
  if (_doTest)    { _doTest = false;    if (_h.testAlarm) _h.testAlarm(_pendingTestGame, _pendingTestTune); }
  if (_doStop)    { _doStop = false;    if (_h.stopAlarm) _h.stopAlarm(); }
}
