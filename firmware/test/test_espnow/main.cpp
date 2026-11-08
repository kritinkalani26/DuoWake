// test_espnow: flash the SAME binary on both boards. Each board prints its MAC on boot (put those in
// secrets.h), picks the peer by comparing its own MAC to MASTER_MAC, joins the hotspot (optional) and pings.
// Press the button on either board -> the OTHER board rings (red ring + beep) for 1 s, then replies STOP.
// Serial shows link status, RTT and channel every 2 s.
#include <Arduino.h>
#include <WiFi.h>
#include "pins.h"
#include "alarm_config.h"
#include "buzzer.h"
#include "button.h"
#include "ring.h"
#include "espnow_link.h"
#include "wifi_manager.h"
#include "secrets_loader.h"

static uint32_t ringUntil = 0;
static uint32_t lastStatus = 0;
static bool amMaster = false;

static void onLink(const LinkMsg& m) {
  switch (m.type) {
    case MSG_RING:
      Ring::fill(CRGB::Red); Ring::show();
      Buzzer::beepPattern(2, 100, 100);
      ringUntil = millis() + 1000;
      break;
    case MSG_STOP:
      Ring::off();
      break;
    case MSG_BUTTON_PRESSED:
      Ring::fill(CRGB::Blue); Ring::show();
      ringUntil = millis() + 300;
      break;
    case MSG_FAIL_BEEP: Buzzer::failBurst(); break;
    case MSG_SET_LEDS: Ring::fillCount(m.arg, CRGB::Green); Ring::show(); break;
    default: break;
  }
}

static void onWifi(const String& ip, uint8_t ch) {
  Serial.printf("[test] WiFi up at http://%s/  channel %u -> locking ESP-NOW\n", ip.c_str(), ch);
  EspNowLink::lockChannel(ch);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== test_espnow ===");
  Buzzer::begin(); Button::begin(); Ring::begin();

  WifiManager::begin(WIFI_SSID, WIFI_PASS, "alarm-test");
  WifiManager::onConnect(onWifi);
  String me = WiFi.macAddress();
  Serial.printf("MY MAC: %s\n", me.c_str());
  amMaster = (me.equalsIgnoreCase(EspNowLink::macString(MASTER_MAC_BYTES)));
  const uint8_t* peer = amMaster ? SLAVE_MAC_BYTES : MASTER_MAC_BYTES;
  Serial.printf("role by MAC: %s, peer %s\n", amMaster ? "MASTER" : "SLAVE (or MAC not in secrets.h)", EspNowLink::macString(peer).c_str());
  EspNowLink::begin(peer);
  EspNowLink::setHandler(onLink);
}

void loop() {
  uint32_t now = millis();
  Button::update(now);
  Buzzer::update(now);
  WifiManager::update(now);
  EspNowLink::setWifiConnected(WifiManager::isConnected());
  EspNowLink::update(now);

  if (Button::pressed()) {
    Serial.println("[test] button -> sending RING to peer");
    EspNowLink::send(MSG_RING);
    EspNowLink::send(MSG_BUTTON_PRESSED);
  }
  if (ringUntil && (int32_t)(now - ringUntil) >= 0) { ringUntil = 0; Ring::off(); }

  if (now - lastStatus >= 2000) {
    lastStatus = now;
    Serial.printf("[test] wifi=%s ip=%s ch=%u | peer %s rtt=%lu ms\n",
      WifiManager::isConnected() ? "up" : "down", WifiManager::ip().c_str(), EspNowLink::channel(),
      EspNowLink::peerLinked() ? "LINKED" : "no link", (unsigned long)EspNowLink::lastRttMs());
  }
}
