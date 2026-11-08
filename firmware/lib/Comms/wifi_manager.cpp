#include "wifi_manager.h"
#include "alarm_config.h"
#include <WiFi.h>

namespace {
  const char* _ssid = nullptr;
  const char* _pass = nullptr;
  bool     _wasConnected = false;
  uint32_t _lastAttempt = 0;
  bool     _attempting = false;
  WifiConnectHandler    _onConnect = nullptr;
  WifiDisconnectHandler _onDisconnect = nullptr;

  void startAttempt(uint32_t now) {
    _lastAttempt = now;
    _attempting = true;
    Serial.printf("[wifi] connecting to '%s'...\n", _ssid);
    WiFi.disconnect(false, false);
    WiFi.begin(_ssid, _pass);
  }
}

void WifiManager::begin(const char* ssid, const char* pass, const char* hostname) {
  _ssid = ssid; _pass = pass;
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);   // we run our own watchdog so ESP-NOW channel handling stays predictable
  WiFi.setHostname(hostname);
  WiFi.setSleep(false);
  startAttempt(millis());
}

void WifiManager::update(uint32_t now) {
  bool c = (WiFi.status() == WL_CONNECTED);
  if (c && !_wasConnected) {
    _wasConnected = true;
    _attempting = false;
    Serial.printf("[wifi] CONNECTED ip=%s ch=%u rssi=%d\n", WiFi.localIP().toString().c_str(), WiFi.channel(), WiFi.RSSI());
    if (_onConnect) _onConnect(WiFi.localIP().toString(), WiFi.channel());
  } else if (!c && _wasConnected) {
    _wasConnected = false;
    _attempting = false;
    Serial.println("[wifi] DISCONNECTED");
    if (_onDisconnect) _onDisconnect();
  }
  if (!c && (now - _lastAttempt) >= WIFI_RETRY_MS) {
    if (_attempting) Serial.println("[wifi] attempt timed out, retrying");
    startAttempt(now);
  }
}

bool WifiManager::isConnected() { return WiFi.status() == WL_CONNECTED; }
String WifiManager::ip() { return isConnected() ? WiFi.localIP().toString() : String("-"); }
int WifiManager::rssi() { return isConnected() ? WiFi.RSSI() : 0; }
uint8_t WifiManager::channel() { return WiFi.channel(); }
void WifiManager::onConnect(WifiConnectHandler h) { _onConnect = h; }
void WifiManager::onDisconnect(WifiDisconnectHandler h) { _onDisconnect = h; }
