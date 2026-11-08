#include "web_options.h"
#include "game_registry.h"
#include "melody_store.h"
#include "builtin_tunes.h"
#include <vector>

namespace {

  void addTune(JsonArray arr, const String& id, const String& name, const char* source, const String& date, const Melody* m) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = id; o["name"] = name; o["source"] = source;
    if (date.length()) o["date"] = date;
    if (m) WebOptions::bars(*m, o["bars"].to<JsonArray>());
  }
}

uint32_t WebOptions::optionsVersion() { return MelodyStore::generation(); }

void WebOptions::bars(const Melody& m, JsonArray out) {
  // Resample the note sequence (by time) into 12 buckets of average pitch.
  constexpr uint8_t N = 12;
  constexpr float LO = 523.0f, HI = 1976.0f;   // C5..B6
  uint32_t total = 0;
  for (uint8_t i = 0; i < m.count; i++) total += m.notes[i].ms;
  if (!total) { for (uint8_t i = 0; i < N; i++) out.add(0); return; }
  float sum[N] = {0}, wsum[N] = {0};
  uint32_t t = 0;
  for (uint8_t i = 0; i < m.count; i++) {
    uint32_t end = t + m.notes[i].ms;
    float v = m.notes[i].freq ? (m.notes[i].freq - LO) / (HI - LO) : 0.0f;
    if (v < 0) v = 0; if (v > 1) v = 1;
    for (uint32_t s = t; s < end; s += 10) {              // 10 ms slices
      uint8_t b = (uint8_t)((uint64_t)s * N / total); if (b >= N) b = N - 1;
      sum[b] += v; wsum[b] += 1;
    }
    t = end;
  }
  for (uint8_t i = 0; i < N; i++) out.add(wsum[i] ? (int)(sum[i] / wsum[i] * 100 + 0.5f) / 100.0f : 0);
}

void WebOptions::build(JsonDocument& out) {
  JsonArray games = out["games"].to<JsonArray>();
  for (uint8_t i = 0; i < GameRegistry::count(); i++) {
    GameInfo gi;
    if (!GameRegistry::info(GameRegistry::nameAt(i), gi)) continue;
    JsonObject o = games.add<JsonObject>();
    o["id"] = gi.id; o["name"] = gi.name; o["tagline"] = gi.tagline;
  }
  { JsonObject o = games.add<JsonObject>(); o["id"] = GameRegistry::PSEUDO_RANDOM;   o["name"] = "Random";      o["tagline"] = "Gemini picks tomorrow's game for you"; o["pseudo"] = true; }
  { JsonObject o = games.add<JsonObject>(); o["id"] = GameRegistry::PSEUDO_SURPRISE; o["name"] = "Surprise me"; o["tagline"] = "Gemini picks the game AND composes a new tune"; o["pseudo"] = true; }

  JsonArray tunes = out["tunes"].to<JsonArray>();
  addTune(tunes, "new",     "New Gemini melody", "pseudo", "", nullptr);
  addTune(tunes, "shuffle", "Shuffle saved",     "pseudo", "", nullptr);
  std::vector<MelodyEntry> v;
  MelodyStore::list(v);
  for (auto& e : v) {
    Melody m;
    bool ok = MelodyStore::loadGenerated(e.file.substring(0, e.file.length() - 5).c_str(), m);
    addTune(tunes, e.id, e.name, "gemini", e.date, ok ? &m : nullptr);
  }
  for (uint8_t i = 0; i < BUILTIN_TUNE_COUNT; i++) {
    Melody m;
    String id = String("builtin:") + BUILTIN_TUNES[i].id;
    bool ok = MelodyStore::loadBuiltin(BUILTIN_TUNES[i].id, m);
    addTune(tunes, id, MelodyStore::tuneName(id), "builtin", "", ok ? &m : nullptr);
  }
  out["version"] = optionsVersion();
}
