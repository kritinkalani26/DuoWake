#include "espnow_link.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Preferences.h>

namespace {
  constexpr uint8_t  QUEUE_LEN      = 16;
  constexpr uint32_t PING_PERIOD_MS = 2000;
  constexpr uint32_t LINK_TIMEOUT_MS = 6000;
  constexpr uint32_t SCAN_HOP_MS    = 3000;   // dwell per channel while scanning
  constexpr uint32_t SCAN_AFTER_MS  = 15000;  // start scanning after this long without PONG (wifi down only)
  constexpr uint8_t  MAX_CHANNEL    = 13;

  uint8_t   _peer[6];
  bool      _peerAdded = false;
  LinkHandler _handler = nullptr;
  MelodyHandler _melodyHandler = nullptr;
  MelodyPacket _melodyIn;
  volatile bool _melodyPending = false;
  uint32_t  _seq = 0;
  uint8_t   _channel = 1;
  bool      _wifiConnected = false;

  uint32_t  _lastPing = 0;
  uint32_t  _lastPong = 0;      // millis() of last PONG (0 = never)
  uint32_t  _lastRtt = 0;
  uint32_t  _lastHop = 0;
  bool      _scanning = false;

  // Single-producer (wifi task) / single-consumer (loop) ring queue.
  volatile uint8_t _qHead = 0, _qTail = 0;
  LinkMsg _queue[QUEUE_LEN];

  const char* typeName(uint8_t t) {
    switch (t) {
      case MSG_RING: return "RING"; case MSG_STOP: return "STOP"; case MSG_BUTTON_PRESSED: return "BUTTON_PRESSED";
      case MSG_FAIL_BEEP: return "FAIL_BEEP"; case MSG_SET_LEDS: return "SET_LEDS"; case MSG_PING: return "PING";
      case MSG_PONG: return "PONG"; case MSG_MELODY: return "MELODY"; default: return "?";
    }
  }

  void onRecv(const uint8_t* mac, const uint8_t* data, int len) {
    if (memcmp(mac, _peer, 6) != 0) return;   // ignore strangers
    if (len == (int)sizeof(MelodyPacket) && data[0] == MSG_MELODY) {
      memcpy(&_melodyIn, data, sizeof(MelodyPacket));
      _melodyPending = true;
      return;
    }
    if (len != (int)sizeof(LinkMsg)) return;
    uint8_t next = (_qHead + 1) % QUEUE_LEN;
    if (next == _qTail) return;               // full: drop
    memcpy(&_queue[_qHead], data, sizeof(LinkMsg));
    _qHead = next;
  }

  void onSent(const uint8_t* mac, esp_now_send_status_t status) {
    (void)mac;
    if (status != ESP_NOW_SEND_SUCCESS) Serial.println("[link] send: no ack");
  }

  void applyChannel(uint8_t ch) {
    if (ch < 1 || ch > MAX_CHANNEL) ch = 1;
    _channel = ch;
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);
  }

  void addPeer() {
    if (_peerAdded) esp_now_del_peer(_peer);
    esp_now_peer_info_t p = {};
    memcpy(p.peer_addr, _peer, 6);
    p.channel = 0;          // 0 = use the current channel
    p.encrypt = false;
    p.ifidx = WIFI_IF_STA;
    esp_err_t r = esp_now_add_peer(&p);
    _peerAdded = (r == ESP_OK);
    if (!_peerAdded) Serial.printf("[link] add_peer failed: %d\n", (int)r);
  }

  void saveChannel(uint8_t ch) {
    Preferences p;
    if (p.begin("link", false)) { p.putUChar("ch", ch); p.end(); }
  }
  uint8_t loadChannel() {
    Preferences p;
    uint8_t ch = 1;
    if (p.begin("link", true)) { ch = p.getUChar("ch", 1); p.end(); }
    return ch;
  }
}

String EspNowLink::macString(const uint8_t mac[6]) {
  char b[18];
  snprintf(b, sizeof(b), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(b);
}

String EspNowLink::myMac() { return WiFi.macAddress(); }

bool EspNowLink::begin(const uint8_t peerMac[6]) {
  memcpy(_peer, peerMac, 6);
  WiFi.setSleep(false);   // modem sleep kills ESP-NOW reliability
  if (esp_now_init() != ESP_OK) { Serial.println("[link] esp_now_init failed"); return false; }
  esp_now_register_recv_cb(onRecv);
  esp_now_register_send_cb(onSent);
  applyChannel(loadChannel());
  addPeer();
  Serial.printf("[link] up. me=%s peer=%s ch=%u\n", myMac().c_str(), macString(_peer).c_str(), _channel);
  return true;
}

void EspNowLink::setHandler(LinkHandler h) { _handler = h; }

bool EspNowLink::send(MsgType type, uint8_t arg) {
  LinkMsg m{ (uint8_t)type, arg, ++_seq, millis() };
  esp_err_t r = esp_now_send(_peer, (const uint8_t*)&m, sizeof(m));
  if (type != MSG_PING && type != MSG_PONG)
    Serial.printf("[link] -> %s arg=%u %s\n", typeName(type), arg, r == ESP_OK ? "" : "(send err)");
  return r == ESP_OK;
}

bool EspNowLink::sendMelody(const Melody& m) {
  MelodyPacket p;
  memset(&p, 0, sizeof(p));
  p.type = MSG_MELODY;
  p.count = m.count > MELODY_MAX_NOTES_STORED ? MELODY_MAX_NOTES_STORED : m.count;
  strlcpy(p.name, m.name, sizeof(p.name));
  for (uint8_t i = 0; i < p.count; i++) { p.notes[i].freq = m.notes[i].freq; p.notes[i].ms = m.notes[i].ms; }
  esp_err_t r = esp_now_send(_peer, (const uint8_t*)&p, sizeof(p));
  Serial.printf("[link] -> MELODY '%s' (%u notes) %s\n", p.name, p.count, r == ESP_OK ? "" : "(send err)");
  return r == ESP_OK;
}

void EspNowLink::setMelodyHandler(MelodyHandler h) { _melodyHandler = h; }

void EspNowLink::lockChannel(uint8_t ch) {
  _scanning = false;
  if (ch >= 1 && ch <= MAX_CHANNEL) {
    _channel = ch;
    saveChannel(ch);
    addPeer();   // re-add so the peer follows the current channel
    Serial.printf("[link] locked to channel %u\n", ch);
  }
}

void EspNowLink::setWifiConnected(bool c) { _wifiConnected = c; }
uint8_t EspNowLink::channel() { return _channel; }
bool EspNowLink::peerLinked() { return _lastPong && (millis() - _lastPong) < LINK_TIMEOUT_MS; }
uint32_t EspNowLink::lastRttMs() { return _lastRtt; }
uint32_t EspNowLink::lastSeenMs() { return _lastPong; }

void EspNowLink::update(uint32_t now) {
  // 1. dispatch queued messages
  while (_qTail != _qHead) {
    LinkMsg m = _queue[_qTail];
    _qTail = (_qTail + 1) % QUEUE_LEN;
    if (m.type == MSG_PING) {
      LinkMsg r{ MSG_PONG, 0, ++_seq, m.sentMs };   // echo sender's timestamp
      esp_now_send(_peer, (const uint8_t*)&r, sizeof(r));
      if (!_lastPong) Serial.println("[link] peer heard (ping)");
      _lastPong = now;  // hearing a ping also proves the link
      continue;
    }
    if (m.type == MSG_PONG) {
      _lastRtt = now - m.sentMs;
      if (_scanning) { Serial.printf("[link] peer found on channel %u\n", _channel); saveChannel(_channel); }
      _scanning = false;
      _lastPong = now;
      continue;
    }
    Serial.printf("[link] <- %s arg=%u\n", typeName(m.type), m.arg);
    if (_handler) _handler(m);
  }

  if (_melodyPending) {
    _melodyPending = false;
    Melody m;
    strlcpy(m.name, _melodyIn.name, sizeof(m.name));
    m.count = _melodyIn.count > MELODY_MAX_NOTES_STORED ? MELODY_MAX_NOTES_STORED : _melodyIn.count;
    for (uint8_t i = 0; i < m.count; i++) { m.notes[i].freq = _melodyIn.notes[i].freq; m.notes[i].ms = _melodyIn.notes[i].ms; }
    Serial.printf("[link] <- MELODY '%s' (%u notes)\n", m.name, m.count);
    if (_melodyHandler) _melodyHandler(m);
  }

  // 2. periodic ping. While WiFi is down the station's connect attempts scan channels, so re-apply ours first.
  if (now - _lastPing >= PING_PERIOD_MS) {
    _lastPing = now;
    if (!_wifiConnected && !_scanning) applyChannel(_channel);
    send(MSG_PING);
  }

  // 3. channel scan when WiFi is down and the peer has been silent for a while
  if (!_wifiConnected) {
    uint32_t silent = _lastPong ? (now - _lastPong) : now;
    if (!_scanning && silent > SCAN_AFTER_MS) {
      _scanning = true;
      _lastHop = now;
      Serial.println("[link] peer silent and WiFi down: scanning channels");
    }
    if (_scanning && now - _lastHop >= SCAN_HOP_MS) {
      _lastHop = now;
      uint8_t next = (_channel % MAX_CHANNEL) + 1;
      applyChannel(next);
      send(MSG_PING);
    }
  } else {
    _scanning = false;
  }
}
