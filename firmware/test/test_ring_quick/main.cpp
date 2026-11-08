// test_ring_quick: minimal ring check. All 12 LEDs blink white 3 times, then one LED chases around once. Repeats.
#include <Arduino.h>
#include "pins.h"
#include "ring.h"

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n=== test_ring_quick: data GPIO%u, %u LEDs ===\n", PIN_RING_DATA, RING_LED_COUNT);
  Ring::begin();
}

void loop() {
  for (int i = 0; i < 3; i++) {
    Serial.printf("blink %d\n", i + 1);
    Ring::fill(CRGB::White); Ring::show(); delay(300);
    Ring::off(); delay(300);
  }
  Serial.println("cycle");
  for (uint8_t i = 0; i < RING_LED_COUNT; i++) {
    Ring::clear(); Ring::set(i, CRGB::Blue); Ring::show(); delay(100);
  }
  Ring::off();
  delay(1500);
}
