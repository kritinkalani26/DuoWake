// ring.h - 12-LED WS2812B ring helpers (FastLED). Non-blocking; call Ring::show() after edits.
#pragma once
#include <Arduino.h>
#include <FastLED.h>

class Ring {
public:
  static void begin();
  static void clear();                     // all off (does not show)
  static void set(uint8_t idx, CRGB c);
  static void fill(CRGB c);
  static void fillCount(uint8_t n, CRGB c, CRGB rest = CRGB::Black); // first n lit (progress)
  static void show();
  static void off();                       // clear + show
  static CRGB* leds();
  static uint8_t count();
  // Simple non-blocking spinner used while idle/ringing: call each loop.
  static void spinner(uint32_t now, CRGB c, uint16_t stepMs = 80);
  static void breathe(uint32_t now, CRGB c);

private:
  static CRGB _leds[];
};
