// test_pinscan: wiring finder. Enables the internal pull-up on every usable GPIO and prints which
// pins go LOW, so you can press the button and see which pin it is really wired to (if any).
// Also prints the live state of the configured PIN_BUTTON once a second and beeps at boot.
#include <Arduino.h>
#include "pins.h"

// Input-capable GPIOs on ESP32 dev boards. 34-39 have no pull-up (they read as LOW/floating) so they are
// listed separately and only reported if they change.
static const uint8_t PULLUP_PINS[] = {4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33};
static const uint8_t NOPULL_PINS[] = {34, 35, 36, 39};
static bool lastState[48];

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== test_pinscan ===");
  Serial.printf("firmware expects the button on GPIO %u (to GND). Press it now and watch which pin reports LOW.\n", PIN_BUTTON);
  for (uint8_t p : PULLUP_PINS) { pinMode(p, INPUT_PULLUP); }
  for (uint8_t p : NOPULL_PINS) { pinMode(p, INPUT); }
  delay(50);
  for (uint8_t p : PULLUP_PINS) lastState[p] = digitalRead(p);
  for (uint8_t p : NOPULL_PINS) lastState[p] = digitalRead(p);
  // pins already LOW at boot = shorted to GND permanently (or a wire sitting on them)
  for (uint8_t p : PULLUP_PINS) if (!lastState[p]) Serial.printf("  GPIO %u is LOW at boot (held to GND?)\n", p);
  // short boot beep on the buzzer pin so we also know the buzzer side is alive
  ledcSetup(0, 2000, 10); ledcAttachPin(PIN_BUZZER, 0); ledcWriteTone(0, 1000); delay(150); ledcWrite(0, 0);
}

void loop() {
  static uint32_t lastPrint = 0;
  for (uint8_t p : PULLUP_PINS) {
    bool s = digitalRead(p);
    if (s != lastState[p]) { lastState[p] = s; Serial.printf("GPIO %u -> %s%s\n", p, s ? "HIGH" : "LOW", p == PIN_BUTTON ? "   <== this is the button pin" : ""); }
  }
  for (uint8_t p : NOPULL_PINS) {
    bool s = digitalRead(p);
    if (s != lastState[p]) { lastState[p] = s; Serial.printf("GPIO %u -> %s (no internal pull-up on this pin)\n", p, s ? "HIGH" : "LOW"); }
  }
  if (millis() - lastPrint > 1000) {
    lastPrint = millis();
    Serial.printf("button pin GPIO %u = %s\n", PIN_BUTTON, digitalRead(PIN_BUTTON) ? "HIGH (not pressed)" : "LOW (PRESSED)");
  }
  delay(5);
}
