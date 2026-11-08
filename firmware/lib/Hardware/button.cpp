#include "button.h"
#include "pins.h"

bool     Button::_stable = false;
bool     Button::_lastRaw = false;
uint32_t Button::_lastChange = 0;
bool     Button::_pressEvent = false;
uint32_t Button::_downSince = 0;

void Button::begin() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  _stable = _lastRaw = (digitalRead(PIN_BUTTON) == LOW);
}

void Button::update(uint32_t now) {
  bool raw = (digitalRead(PIN_BUTTON) == LOW);
  if (raw != _lastRaw) { _lastRaw = raw; _lastChange = now; }
  if (raw != _stable && (now - _lastChange) >= BUTTON_DEBOUNCE_MS) {
    _stable = raw;
    if (_stable) { _pressEvent = true; _downSince = now; }
  }
}

bool Button::pressed() {
  bool e = _pressEvent;
  _pressEvent = false;
  return e;
}

bool Button::isDown() { return _stable; }
uint32_t Button::heldMs() { return _stable ? (millis() - _downSince) : 0; }
