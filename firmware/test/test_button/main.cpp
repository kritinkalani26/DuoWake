// test_button: prints each debounced press with a counter, and lights the ring while held.
#include <Arduino.h>
#include "pins.h"
#include "button.h"
#include "ring.h"
#include "buzzer.h"

static uint32_t presses = 0;
static bool wasDown = false;

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== test_button ===");
  Serial.printf("button=GPIO%u (INPUT_PULLUP, press -> GND), debounce=%u ms\n", PIN_BUTTON, BUTTON_DEBOUNCE_MS);
  Button::begin();
  Ring::begin();
  Buzzer::begin();
}

void loop() {
  uint32_t now = millis();
  Button::update(now);
  Buzzer::update(now);
  if (Button::pressed()) {
    presses++;
    Serial.printf("press #%lu at %lu ms\n", (unsigned long)presses, (unsigned long)now);
    Buzzer::successChirp();
  }
  bool down = Button::isDown();
  if (down != wasDown) {
    wasDown = down;
    if (down) { Ring::fill(CRGB::White); Ring::show(); }
    else { Serial.printf("released after %lu ms\n", (unsigned long)(now)); Ring::off(); }
  }
}
