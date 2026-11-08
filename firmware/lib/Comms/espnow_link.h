// espnow_link.h - typed ESP-NOW messages between MASTER and SLAVE.
// Receive callbacks run on the WiFi task; messages are queued and dispatched from update() on the main loop.
// Channel handling: ESP-NOW only works if both boards sit on the same WiFi channel. After the station
// connects we lock to the hotspot's channel and remember it in NVS; if WiFi is down we use the remembered
// channel, and if the peer stays silent we slowly scan channels until a PONG arrives.
#pragma once
#include <Arduino.h>
#include "melody.h"

enum MsgType : uint8_t {
  MSG_RING = 1,         // start ringing (melody + red ring)
  MSG_STOP,             // go silent and dark
  MSG_BUTTON_PRESSED,   // slave -> master: button pressed (arg unused)
  MSG_FAIL_BEEP,        // play fail burst
  MSG_SET_LEDS,         // arg = number of LEDs to light green (progress)
  MSG_PING,
  MSG_PONG,
  MSG_MELODY,           // master -> slave: full melody (MelodyPacket, not LinkMsg)
};

struct __attribute__((packed)) LinkMsg {
  uint8_t  type;
  uint8_t  arg;
  uint32_t seq;
  uint32_t sentMs;   // sender millis(), echoed in PONG for RTT
};

// Melody transfer so SLAVE rings with the same tune as MASTER (fits in one 250-byte ESP-NOW frame).
struct __attribute__((packed)) MelodyPacket {
  uint8_t  type;        // MSG_MELODY
  uint8_t  count;
  char     name[32];
  struct { uint16_t freq; uint16_t ms; } notes[MELODY_MAX_NOTES_STORED];
};

typedef void (*LinkHandler)(const LinkMsg& msg);
typedef void (*MelodyHandler)(const Melody& m);

namespace EspNowLink {
  // Must be called after WiFi.mode(WIFI_STA). peerMac = the other clock.
  bool begin(const uint8_t peerMac[6]);
  void setHandler(LinkHandler h);
  bool send(MsgType type, uint8_t arg = 0);
  bool sendMelody(const Melody& m);
  void setMelodyHandler(MelodyHandler h);
  void update(uint32_t now);               // dispatch queue, send pings, scan channels if needed

  // Call when the station (re)connects: lock ESP-NOW to this channel and persist it.
  void lockChannel(uint8_t ch);
  // Call with the station state each loop so the link knows whether it may scan channels.
  void setWifiConnected(bool connected);
  uint8_t channel();

  bool     peerLinked();                   // PONG seen in the last few seconds
  uint32_t lastRttMs();
  uint32_t lastSeenMs();
  String   macString(const uint8_t mac[6]);
  String   myMac();
}
