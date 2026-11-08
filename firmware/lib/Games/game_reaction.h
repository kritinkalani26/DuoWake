// game_reaction.h - REACTION: a random LED flashes briefly at random intervals; press during the flash.
// 5 hits in a row to win. Miss (too slow) or press with nothing lit resets the streak.
#pragma once
#include "game.h"

class GameReaction : public Game {
public:
  void begin() override;
  void update(uint32_t now) override;
  void onButton(uint8_t clockId) override;
  bool isComplete() override;
  void reset() override;
  const char* name() override { return "reaction"; }
  const char* displayName() override { return "Flash Tag"; }
  const char* tagline() override { return "Hit the button when the light flashes"; }

private:
  enum State { IDLE, GAP, LIT, DONE };
  State    _state = IDLE;
  uint8_t  _streak = 0;
  uint8_t  _litIdx = 0;
  uint16_t _windowMs = 0;
  uint32_t _deadline = 0;

  void showStreak();
  void scheduleNext(uint32_t now);
  void miss(const char* why);
};
