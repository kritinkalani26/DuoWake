#include "slave_app.h"
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

namespace {
  bool     _ringing = false;
  uint32_t _lastHeartbeat = 0;
  uint32_t _flashUntil = 0;
  uint8_t  _progress = 0;

  void showProgress() {
    Ring::fillCount(_progress, CRGB::Green);
    Ring::show();
  }

  void silence() {
    _ringing = false;
    MelodyPlayer::stop();
    Buzzer::stopPattern();
    showProgress();
    Serial.println("[slave] silent");
  }

  void onLink(const LinkMsg& m) {
    switch (m.type) {
      case MSG_RING:
        _ringing = true;
        MelodyPlayer::play();
        Serial.println("[slave] RINGING");
        break;
      case MSG_STOP:
        silence();
        break;
      case MSG_FAIL_BEEP:
        Buzzer::failBurst();
        Ring::fill(CRGB(80, 0, 0)); Ring::show();
        _flashUntil = millis() + 600;
        Serial.println("[slave] fail beep");
        break;
      case MSG_SET_LEDS:
        _progress = m.arg > Ring::count() ? Ring::count() : m.arg;
        if (!_ringing) showProgress();
        break;
      default: break;
    }
  }

  void onMelody(const Melody& m) {
    MelodyPlayer::load(m);
  }

  void onWifiUp(const String& ip, uint8_t ch) {
    Serial.printf("[slave] wifi ip=%s ch=%u -> re-locking ESP-NOW channel\n", ip.c_str(), ch);
    EspNowLink::lockChannel(ch);
  }
}

void SlaveApp::begin() {
  Serial.println("\n===== DuoWake : SLAVE (clock B) =====");
  randomSeed(esp_random());
  Buzzer::begin(); Button::begin(); Ring::begin();
  MelodyStore::begin();
  Melody m;
  MelodyStore::loadBuiltin("siren_stairs", m);   // default until MASTER sends the real one
  MelodyPlayer::load(m);

  WifiManager::begin(WIFI_SSID, WIFI_PASS, "alarm-slave");
  WifiManager::onConnect(onWifiUp);
  Serial.printf("[slave] MY MAC: %s\n", EspNowLink::myMac().c_str());
  EspNowLink::begin(MASTER_MAC_BYTES);
  EspNowLink::setHandler(onLink);
  EspNowLink::setMelodyHandler(onMelody);
  Serial.println("[slave] ready, waiting for MASTER");
}

void SlaveApp::loop() {
  uint32_t now = millis();
  Button::update(now);
  Buzzer::update(now);
  MelodyPlayer::update(now);
  WifiManager::update(now);
  EspNowLink::setWifiConnected(WifiManager::isConnected());
  EspNowLink::update(now);

  if (_ringing) Ring::spinner(now, CRGB::Red, 60);
  if (_flashUntil && (int32_t)(now - _flashUntil) >= 0) { _flashUntil = 0; if (!_ringing) showProgress(); }

  if (Button::pressed()) {
    Serial.printf("[slave] button pressed (%s) -> BUTTON_PRESSED\n", _ringing ? "ringing" : "idle");
    EspNowLink::send(MSG_BUTTON_PRESSED);
    if (_ringing) silence();
  }

  if (now - _lastHeartbeat >= 10000) {
    _lastHeartbeat = now;
    Serial.printf("[slave] %s | wifi %s ch %u | master %s rtt %lu ms\n",
      _ringing ? "RINGING" : "quiet", WifiManager::isConnected() ? "up" : "down", EspNowLink::channel(),
      EspNowLink::peerLinked() ? "linked" : "NO LINK", (unsigned long)EspNowLink::lastRttMs());
  }
}
