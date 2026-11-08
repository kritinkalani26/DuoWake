// game_sprint.h - SPRINT: run between the two clocks. MASTER rings; press it -> SLAVE rings; run there and
// press it -> MASTER rings again... SPRINT_LEGS legs (A->B->A->B->A). Each leg has a time limit derived from
// the configured distance. Too slow = fail burst on both clocks and restart from leg 1.
// This game runs on MASTER only; SLAVE just obeys RING/STOP/FAIL_BEEP/SET_LEDS and reports BUTTON_PRESSED.
#pragma once
#include "game.h"

class GameSprint : public Game {
public:
  void configure(const GameParams& p) override;
  void begin() override;
  void update(uint32_t now) override;
  void onButton(uint8_t clockId) override;
  bool isComplete() override;
  void reset() override;
  const char* name() override { return "sprint"; }
  const char* displayName() override { return "Dash"; }
  const char* tagline() override { return "Run between the clocks before the timer runs out"; }

private:
  enum State { IDLE, RING_A, RUN_TO_B, RUN_TO_A, FAIL_PAUSE, DONE };
  State    _state = IDLE;
  uint8_t  _legsDone = 0;
  uint32_t _legStart = 0;
  uint32_t _allowedMs = 10000;
  uint32_t _pauseEnd = 0;
  float    _distance = 5.0f;

  void ringMaster(bool on);
  void showProgress();
  void fail(uint32_t legMs);
  bool legOk(uint32_t now, uint32_t& legMs);
};
