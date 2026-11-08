// pins.h - THE ONLY place GPIO numbers live. Nothing else may hardcode a pin.
// Matches the real schematic (one passive buzzer per clock).
#pragma once
#include <stdint.h>

// WS2812B 12-LED ring, 5V. Data through a 330 Ω series resistor.
constexpr uint8_t PIN_RING_DATA   = 27;
constexpr uint8_t RING_LED_COUNT  = 12;
constexpr uint8_t RING_BRIGHTNESS = 60;   // global brightness cap (0-255) to limit current

// Single PASSIVE buzzer, driven by LEDC PWM (tone). GPIO -> 1k -> 2N3904 base, low-side switch.
// HIGH = buzzer energised, so the PWM waveform drives it directly.
constexpr uint8_t PIN_BUZZER          = 25;
constexpr uint8_t BUZZER_LEDC_CHANNEL = 0;

// Push button to GND, INPUT_PULLUP, pressed = LOW, debounced in software
constexpr uint8_t  PIN_BUTTON         = 33;
constexpr uint16_t BUTTON_DEBOUNCE_MS = 30;
