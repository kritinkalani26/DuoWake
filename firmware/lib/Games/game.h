// game.h - common interface every wake-up game implements.
// Games are non-blocking: begin() sets state, update(now) advances it from loop().
// Games drive the buzzers (melody + fail bursts) and the ring themselves.
#pragma once
#include <Arduino.h>

struct GameParams {
  float distanceMeters = 5.0f;   // used by sprint
};

class Game {
public:
  virtual ~Game() {}
  // Optional: receive user settings before begin(). Default ignores them.
  virtual void configure(const GameParams& p) { (void)p; }
  // Start (or restart) the game from scratch. Starts the alarm sound.
  virtual void begin() = 0;
  // Called every loop() with millis(). Must not block.
  virtual void update(uint32_t now) = 0;
  // A button was pressed on the given clock (CLOCK_MASTER / CLOCK_SLAVE).
  virtual void onButton(uint8_t clockId) = 0;
  // True once the user has fully completed the game.
  virtual bool isComplete() = 0;
  // Clear all state and silence outputs (does not start a new round).
  virtual void reset() = 0;
  // Stable machine id: "reaction", "rotate", "sprint". The UI and API only ever use this.
  virtual const char* name() = 0;
  // Human-facing name and one-line tagline shown on the phone.
  virtual const char* displayName() = 0;
  virtual const char* tagline() = 0;
};
