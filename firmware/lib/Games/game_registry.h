// game_registry.h - id -> factory. To add a game: write game_xxx.h/.cpp and add ONE line in game_registry.cpp.
// Pseudo ids "random" and "surprise" are not games; they are resolved by AlarmPrep into a real game id.
#pragma once
#include "game.h"

struct GameInfo {
  String id;
  String name;      // display name
  String tagline;
};

namespace GameRegistry {
  // Returns a heap-allocated game or nullptr if the id is unknown. Caller owns it.
  Game* create(const char* id);
  bool  exists(const char* id);
  uint8_t count();
  const char* nameAt(uint8_t i);      // id at index
  const char* randomName();           // random id
  // Display name + tagline for an id (creates a throwaway instance). ok=false if unknown.
  bool info(const char* id, GameInfo& out);
  // Comma separated ids for prompts: "reaction,rotate,sprint"
  String namesCsv();

  // Pseudo options the scheduler understands (not real games).
  constexpr const char* PSEUDO_RANDOM   = "random";
  constexpr const char* PSEUDO_SURPRISE = "surprise";
  bool isPseudo(const char* id);
}
