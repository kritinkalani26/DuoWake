// test_ring: cycles through ring patterns so you can verify wiring, color order, and brightness.
// Expect: red/green/blue fills, then a progress fill 0..12, then a spinner, then breathing.
#include <Arduino.h>
#include "pins.h"
#include "ring.h"

static uint32_t t0 = 0;
static uint8_t phase = 0;
static uint8_t progress = 0;

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== test_ring ===");
  Serial.printf("data=GPIO%u leds=%u brightness=%u\n", PIN_RING_DATA, RING_LED_COUNT, RING_BRIGHTNESS);
  Ring::begin();
  t0 = millis();
}

void loop() {
  uint32_t now = millis();
  switch (phase) {
    case 0: Ring::fill(CRGB::Red);   Ring::show(); if (now - t0 > 1000) { phase++; t0 = now; Serial.println("red -> green"); } break;
    case 1: Ring::fill(CRGB::Green); Ring::show(); if (now - t0 > 1000) { phase++; t0 = now; Serial.println("green -> blue"); } break;
    case 2: Ring::fill(CRGB::Blue);  Ring::show(); if (now - t0 > 1000) { phase++; t0 = now; Serial.println("blue -> progress"); } break;
    case 3:
      if (now - t0 > 250) {
        t0 = now;
        Ring::fillCount(progress, CRGB::Green);
        Ring::show();
        Serial.printf("progress %u/%u\n", progress, RING_LED_COUNT);
        if (++progress > RING_LED_COUNT) { progress = 0; phase++; Serial.println("progress -> spinner"); }
      }
      break;
    case 4: Ring::spinner(now, CRGB::Cyan); if (now - t0 > 4000) { phase++; t0 = now; Serial.println("spinner -> breathe"); } break;
    case 5: Ring::breathe(now, CRGB::Magenta); if (now - t0 > 4000) { phase = 0; t0 = now; Serial.println("breathe -> red"); } break;
  }
}
