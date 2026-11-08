// wifi_manager.h - non-blocking station connection to the phone hotspot with a 10 s reconnect watchdog.
// Fires onConnect(ip, channel) on every (re)connect so callers can lock ESP-NOW, start NTP, print the IP.
#pragma once
#include <Arduino.h>

typedef void (*WifiConnectHandler)(const String& ip, uint8_t channel);
typedef void (*WifiDisconnectHandler)();

namespace WifiManager {
  void begin(const char* ssid, const char* pass, const char* hostname);
  void update(uint32_t now);
  bool isConnected();
  String ip();
  int    rssi();
  uint8_t channel();
  void onConnect(WifiConnectHandler h);
  void onDisconnect(WifiDisconnectHandler h);
}
