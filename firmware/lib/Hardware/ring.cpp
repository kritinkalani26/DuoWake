#include "ring.h"
#include "pins.h"

CRGB Ring::_leds[RING_LED_COUNT];

void Ring::begin() {
  FastLED.addLeds<WS2812B, PIN_RING_DATA, GRB>(_leds, RING_LED_COUNT);
  FastLED.setBrightness(RING_BRIGHTNESS);
  off();
}

void Ring::clear() { fill_solid(_leds, RING_LED_COUNT, CRGB::Black); }
void Ring::set(uint8_t idx, CRGB c) { if (idx < RING_LED_COUNT) _leds[idx] = c; }
void Ring::fill(CRGB c) { fill_solid(_leds, RING_LED_COUNT, c); }
void Ring::fillCount(uint8_t n, CRGB c, CRGB rest) {
  for (uint8_t i = 0; i < RING_LED_COUNT; i++) _leds[i] = (i < n) ? c : rest;
}
void Ring::show() { FastLED.show(); }
void Ring::off() { clear(); show(); }
CRGB* Ring::leds() { return _leds; }
uint8_t Ring::count() { return RING_LED_COUNT; }

void Ring::spinner(uint32_t now, CRGB c, uint16_t stepMs) {
  uint8_t head = (now / stepMs) % RING_LED_COUNT;
  clear();
  for (uint8_t t = 0; t < 4; t++) {
    uint8_t i = (head + RING_LED_COUNT - t) % RING_LED_COUNT;
    CRGB d = c; d.nscale8(255 >> t);
    _leds[i] = d;
  }
  show();
}

void Ring::breathe(uint32_t now, CRGB c) {
  uint8_t v = beatsin8(20, 10, 255);
  CRGB d = c; d.nscale8(v);
  fill(d);
  show();
}
