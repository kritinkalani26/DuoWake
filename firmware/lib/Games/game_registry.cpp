#include "game_registry.h"
#include "game_reaction.h"
#include "game_rotate.h"
#include "game_sprint.h"

namespace {
  struct Entry { const char* id; Game* (*make)(); };

  // ---- ADD NEW GAMES HERE (one line each). Display name + tagline live in the game class. ----
  const Entry REGISTRY[] = {
    { "reaction", []() -> Game* { return new GameReaction(); } },
    { "rotate",   []() -> Game* { return new GameRotate(); } },
    { "sprint",   []() -> Game* { return new GameSprint(); } },
  };
  constexpr uint8_t REGISTRY_LEN = sizeof(REGISTRY) / sizeof(REGISTRY[0]);
}

Game* GameRegistry::create(const char* id) {
  if (!id) return nullptr;
  for (uint8_t i = 0; i < REGISTRY_LEN; i++)
    if (strcmp(REGISTRY[i].id, id) == 0) return REGISTRY[i].make();
  return nullptr;
}

bool GameRegistry::exists(const char* id) {
  if (!id) return false;
  for (uint8_t i = 0; i < REGISTRY_LEN; i++) if (strcmp(REGISTRY[i].id, id) == 0) return true;
  return false;
}

bool GameRegistry::isPseudo(const char* id) {
  return id && (strcmp(id, PSEUDO_RANDOM) == 0 || strcmp(id, PSEUDO_SURPRISE) == 0);
}

bool GameRegistry::info(const char* id, GameInfo& out) {
  Game* g = create(id);
  if (!g) return false;
  out.id = g->name();
  out.name = g->displayName();
  out.tagline = g->tagline();
  delete g;
  return true;
}

uint8_t GameRegistry::count() { return REGISTRY_LEN; }
const char* GameRegistry::nameAt(uint8_t i) { return i < REGISTRY_LEN ? REGISTRY[i].id : ""; }
const char* GameRegistry::randomName() { return REGISTRY[random(REGISTRY_LEN)].id; }

String GameRegistry::namesCsv() {
  String s;
  for (uint8_t i = 0; i < REGISTRY_LEN; i++) { if (i) s += ","; s += REGISTRY[i].id; }
  return s;
}
