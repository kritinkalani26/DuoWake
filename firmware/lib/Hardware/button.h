// button.h - debounced push button (INPUT_PULLUP, active LOW).
// Call Button::update(now) each loop; pressed() returns true exactly once per press.
#pragma once
#include <Arduino.h>

class Button {
public:
  static void begin();
  static void update(uint32_t now);
  static bool pressed();      // edge: true once per press (consumed on read)
  static bool isDown();       // current debounced level
  static uint32_t heldMs();   // how long currently held (0 if up)

private:
  static bool     _stable;
  static bool     _lastRaw;
  static uint32_t _lastChange;
  static bool     _pressEvent;
  static uint32_t _downSince;
};
