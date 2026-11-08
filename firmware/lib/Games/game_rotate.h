// game_rotate.h - ROTATE: one LED chases around the ring; LED 0 is the target (blue).
// Press when the chaser is on 11, 0 or 1. 5 hits to win, chase speeds up each hit; a wrong press resets.
#pragma once
#include "game.h"

class GameRotate : public Game {
public:
  void begin() override;
  void update(uint32_t now) override;
  void onButton(uint8_t clockId) override;
  bool isComplete() override;
  void reset() override;
  const char* name() override { return "rotate"; }
  const char* displayName() override { return "Lighthouse"; }
  const char* tagline() override { return "Catch the spinning light on the arrow"; }

private:
  enum State { IDLE, CHASE, PAUSE, DONE };
  State    _state = IDLE;
  uint8_t  _streak = 0;
  uint8_t  _pos = 0;
  uint16_t _stepMs = 0;
  uint32_t _nextStep = 0;
  uint32_t _pauseEnd = 0;

  void draw();
  void startChase(uint32_t now);
  bool onTarget() const;
};
