#include "melody_store.h"
#include "builtin_tunes.h"
#include "alarm_config.h"
#include <LittleFS.h>
#include <algorithm>

namespace {
  constexpr uint8_t HISTORY_KEEP = 30;
  uint32_t _generation = 1;

  void loadHistory(JsonDocument& doc) {
    File f = LittleFS.open(HISTORY_FILE, "r");
    if (f) { deserializeJson(doc, f); f.close(); }
    if (!doc.is<JsonArray>()) doc.to<JsonArray>();
  }

  bool writeHistory(JsonDocument& doc) {
    File f = LittleFS.open(HISTORY_FILE, "w");
    if (!f) { Serial.println("[store] cannot write history"); return false; }
    serializeJson(doc, f);
    f.close();
    return true;
  }

  String builtinName(uint8_t i) {
    JsonDocument d;
    if (deserializeJson(d, BUILTIN_TUNES[i].json)) return BUILTIN_TUNES[i].id;
    return String((const char*)(d["name"] | BUILTIN_TUNES[i].id));
  }
}

bool MelodyStore::begin() {
  if (!LittleFS.begin(false)) {
    Serial.println("[store] LittleFS mount failed, formatting...");
    if (!LittleFS.begin(true)) { Serial.println("[store] LittleFS format failed"); return false; }
  }
  if (!LittleFS.exists(MELODY_DIR)) LittleFS.mkdir(MELODY_DIR);
  Serial.printf("[store] LittleFS ok, used %u / %u bytes\n", (unsigned)LittleFS.usedBytes(), (unsigned)LittleFS.totalBytes());
  return true;
}

bool MelodyStore::isPseudoTune(const String& id) { return id == "new" || id == "shuffle"; }

bool MelodyStore::loadBuiltin(const char* slug, Melody& out) {
  for (uint8_t i = 0; i < BUILTIN_TUNE_COUNT; i++) {
    if (strcmp(BUILTIN_TUNES[i].id, slug) == 0) {
      String err;
      if (!MelodyFormat::fromJson(BUILTIN_TUNES[i].json, out, &err)) {
        Serial.printf("[store] builtin '%s' invalid: %s\n", slug, err.c_str());
        return false;
      }
      return true;
    }
  }
  return false;
}

bool MelodyStore::randomBuiltin(Melody& out, String* idOut) {
  uint8_t i = random(BUILTIN_TUNE_COUNT);
  if (idOut) *idOut = String("builtin:") + BUILTIN_TUNES[i].id;
  return loadBuiltin(BUILTIN_TUNES[i].id, out);
}

bool MelodyStore::loadGenerated(const char* fileBase, Melody& out) {
  String path = String(MELODY_DIR) + "/" + fileBase + ".json";
  File f = LittleFS.open(path, "r");
  if (!f) { Serial.printf("[store] missing %s\n", path.c_str()); return false; }
  String json = f.readString();
  f.close();
  String err;
  if (!MelodyFormat::fromJson(json.c_str(), out, &err)) {
    Serial.printf("[store] %s invalid: %s\n", path.c_str(), err.c_str());
    return false;
  }
  return true;
}

bool MelodyStore::randomGenerated(Melody& out, String* idOut) {
  std::vector<MelodyEntry> v;
  list(v);
  if (v.empty()) return false;
  // try a few in case one file is corrupt
  for (uint8_t attempt = 0; attempt < 3; attempt++) {
    const MelodyEntry& e = v[random(v.size())];
    if (loadGenerated(e.file.substring(0, e.file.length() - 5).c_str(), out)) {
      if (idOut) *idOut = e.id;
      return true;
    }
  }
  return false;
}

bool MelodyStore::resolve(const String& tuneId, Melody& out, String* resolvedId) {
  if (tuneId.startsWith("builtin:")) {
    if (loadBuiltin(tuneId.c_str() + 8, out)) { if (resolvedId) *resolvedId = tuneId; return true; }
  } else if (tuneId.startsWith("gen:")) {
    if (loadGenerated(tuneId.c_str() + 4, out)) { if (resolvedId) *resolvedId = tuneId; return true; }
  } else if (isPseudoTune(tuneId)) {
    if (randomGenerated(out, resolvedId)) { Serial.printf("[store] %s -> %s\n", tuneId.c_str(), resolvedId ? resolvedId->c_str() : "?"); return true; }
    Serial.printf("[store] %s: no saved melodies yet, using a built-in\n", tuneId.c_str());
    return randomBuiltin(out, resolvedId);
  }
  Serial.printf("[store] could not resolve '%s', using random builtin\n", tuneId.c_str());
  return randomBuiltin(out, resolvedId);
}

bool MelodyStore::isValidTuneId(const String& id) {
  if (isPseudoTune(id)) return true;
  if (id.startsWith("builtin:")) {
    for (uint8_t i = 0; i < BUILTIN_TUNE_COUNT; i++) if (id == String("builtin:") + BUILTIN_TUNES[i].id) return true;
    return false;
  }
  if (id.startsWith("gen:")) {
    if (id.indexOf("..") >= 0 || id.indexOf('/') >= 0) return false;
    return LittleFS.exists(String(MELODY_DIR) + "/" + id.substring(4) + ".json");
  }
  return false;
}

String MelodyStore::tuneName(const String& id) {
  if (id == "new") return "New Gemini melody";
  if (id == "shuffle") return "Shuffle saved";
  if (id.startsWith("builtin:")) {
    for (uint8_t i = 0; i < BUILTIN_TUNE_COUNT; i++) if (id == String("builtin:") + BUILTIN_TUNES[i].id) return builtinName(i);
    return "";
  }
  if (id.startsWith("gen:")) {
    Melody m;
    if (loadGenerated(id.c_str() + 4, m)) return String(m.name);
  }
  return "";
}

uint32_t MelodyStore::generation() { return _generation; }

String MelodyStore::slugify(const String& s) {
  String o;
  for (size_t i = 0; i < s.length() && o.length() < 24; i++) {
    char c = tolower(s[i]);
    if (isalnum(c)) o += c;
    else if (o.length() && o[o.length() - 1] != '-') o += '-';
  }
  while (o.endsWith("-")) o.remove(o.length() - 1);
  if (!o.length()) o = "melody";
  return o;
}

String MelodyStore::save(const Melody& m, const String& date) {
  String base = date + "_" + slugify(m.name);
  String fileBase = base;
  String path = String(MELODY_DIR) + "/" + fileBase + ".json";
  uint8_t k = 2;
  while (LittleFS.exists(path) && k < 20) {
    fileBase = base + "-" + String(k++);
    path = String(MELODY_DIR) + "/" + fileBase + ".json";
  }
  File f = LittleFS.open(path, "w");
  if (!f) { Serial.printf("[store] cannot write %s\n", path.c_str()); return ""; }
  f.print(MelodyFormat::toJson(m));
  f.close();
  Serial.printf("[store] saved melody '%s' -> %s\n", m.name, path.c_str());
  return "gen:" + fileBase;
}

void MelodyStore::list(std::vector<MelodyEntry>& out) {
  out.clear();
  File dir = LittleFS.open(MELODY_DIR);
  if (!dir || !dir.isDirectory()) return;
  File f = dir.openNextFile();
  while (f) {
    String fname = f.name();
    int slash = fname.lastIndexOf('/');
    if (slash >= 0) fname = fname.substring(slash + 1);
    if (fname.endsWith(".json")) {
      MelodyEntry e;
      e.file = fname;
      e.id = "gen:" + fname.substring(0, fname.length() - 5);
      e.date = fname.substring(0, 10);
      JsonDocument doc;
      if (!deserializeJson(doc, f)) e.name = String((const char*)(doc["name"] | fname.c_str()));
      else e.name = fname;
      out.push_back(e);
    }
    f = dir.openNextFile();
  }
  std::sort(out.begin(), out.end(), [](const MelodyEntry& a, const MelodyEntry& b) { return a.file > b.file; });
}

String MelodyStore::allNames() {
  String s;
  for (uint8_t i = 0; i < BUILTIN_TUNE_COUNT; i++) { if (s.length()) s += ", "; s += builtinName(i); }
  std::vector<MelodyEntry> v;
  list(v);
  for (auto& e : v) { if (s.length()) s += ", "; s += e.name; }
  return s;
}

// ---- History ----
void MelodyStore::addHistory(const String& date, const String& gameId, const String& tuneId, const String& tuneName, const String& reason) {
  JsonDocument doc;
  loadHistory(doc);
  JsonArray arr = doc.as<JsonArray>();
  JsonObject e = arr.add<JsonObject>();
  e["date"] = date; e["game"] = gameId; e["tuneId"] = tuneId; e["tune"] = tuneName; e["reason"] = reason; e["completed"] = false;
  while (arr.size() > HISTORY_KEEP) arr.remove(0);
  if (writeHistory(doc))
    Serial.printf("[store] history += %s game=%s tune=%s\n", date.c_str(), gameId.c_str(), tuneName.c_str());
}

void MelodyStore::markLastCompleted() {
  JsonDocument doc;
  loadHistory(doc);
  JsonArray arr = doc.as<JsonArray>();
  if (arr.size() == 0) return;
  arr[arr.size() - 1]["completed"] = true;
  writeHistory(doc);
}

String MelodyStore::historyJson() {
  JsonDocument doc;
  loadHistory(doc);
  String s;
  serializeJson(doc, s);
  return s;
}

String MelodyStore::lastGame() {
  JsonDocument doc;
  loadHistory(doc);
  JsonArray arr = doc.as<JsonArray>();
  if (arr.size() == 0) return "";
  return String((const char*)(arr[arr.size() - 1]["game"] | ""));
}
