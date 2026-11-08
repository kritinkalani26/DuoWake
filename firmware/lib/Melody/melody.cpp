#include "melody.h"
#include "buzzer.h"
#include "alarm_config.h"
#include <string.h>

// ---- Note table: C5..B6 incl sharps. Nothing below C5: passive buzzers are too quiet under ~500 Hz. ----
namespace {
  struct NoteDef { const char* n; uint16_t f; };
  const NoteDef NOTE_TABLE[] = {
    {"C5",523},{"C#5",554},{"D5",587},{"D#5",622},{"E5",659},{"F5",698},{"F#5",740},{"G5",784},{"G#5",831},{"A5",880},{"A#5",932},{"B5",988},
    {"C6",1047},{"C#6",1109},{"D6",1175},{"D#6",1245},{"E6",1319},{"F6",1397},{"F#6",1480},{"G6",1568},{"G#6",1661},{"A6",1760},{"A#6",1865},{"B6",1976},
  };
  constexpr size_t NOTE_TABLE_LEN = sizeof(NOTE_TABLE) / sizeof(NOTE_TABLE[0]);

  // Normalize "c#5", "Db5" etc. to canonical table form. Flats are mapped to sharps.
  bool normalize(const char* in, char* out, size_t outLen) {
    size_t L = strlen(in);
    if (L < 2 || L > 3 || outLen < 5) return false;
    char letter = toupper(in[0]);
    if (letter < 'A' || letter > 'G') return false;
    bool sharp = false, flat = false;
    char oct;
    if (L == 3) {
      if (in[1] == '#') sharp = true;
      else if (in[1] == 'b' || in[1] == 'B') flat = true;
      else return false;
      oct = in[2];
    } else {
      oct = in[1];
    }
    if (oct < '0' || oct > '9') return false;
    if (flat) {
      switch (letter) {
        case 'C': letter = 'B'; oct--; break;          // Cb -> B (octave below)
        case 'F': letter = 'E'; break;                  // Fb -> E
        case 'D': letter = 'C'; sharp = true; break;
        case 'E': letter = 'D'; sharp = true; break;
        case 'G': letter = 'F'; sharp = true; break;
        case 'A': letter = 'G'; sharp = true; break;
        case 'B': letter = 'A'; sharp = true; break;
      }
    }
    if (sharp && (letter == 'E' || letter == 'B')) return false;
    snprintf(out, outLen, "%c%s%c", letter, sharp ? "#" : "", oct);
    return true;
  }
}

bool MelodyFormat::noteToFreq(const char* name, uint16_t& freqOut) {
  if (!name) return false;
  if (strcasecmp(name, "REST") == 0 || strcasecmp(name, "R") == 0) { freqOut = 0; return true; }
  char norm[5];
  if (!normalize(name, norm, sizeof(norm))) return false;
  for (size_t i = 0; i < NOTE_TABLE_LEN; i++) {
    if (strcmp(NOTE_TABLE[i].n, norm) == 0) { freqOut = NOTE_TABLE[i].f; return true; }
  }
  return false;
}

const char* MelodyFormat::freqToName(uint16_t freq) {
  if (freq == 0) return "REST";
  for (size_t i = 0; i < NOTE_TABLE_LEN; i++) if (NOTE_TABLE[i].f == freq) return NOTE_TABLE[i].n;
  return "REST";
}

// Lenient parser: Gemini output is repaired rather than rejected wherever possible, because a rejected
// melody means the alarm silently falls back to a built-in tune. Repairs: notes outside C5..B6 are moved
// into range by octave, unknown note names are dropped, durations are clamped, long melodies are cut to
// MELODY_MAX_NOTES. Only "fewer than MELODY_MIN_NOTES usable notes" is a hard failure.
bool MelodyFormat::fromJson(JsonVariantConst v, Melody& out, String* err) {
  auto fail = [&](const String& s) { if (err) *err = s; return false; };
  if (!v.is<JsonObjectConst>()) return fail("root not object");
  JsonObjectConst o = v.as<JsonObjectConst>();
  const char* name = o["name"] | "";
  if (!name || !*name) name = "Gemini Tune";
  JsonArrayConst notes = o["notes"].as<JsonArrayConst>();
  if (notes.isNull()) return fail("missing notes");
  Melody tmp;
  strlcpy(tmp.name, name, sizeof(tmp.name));
  tmp.count = 0;
  uint8_t dropped = 0, fixed = 0;
  for (JsonVariantConst nv : notes) {
    if (tmp.count >= MELODY_MAX_NOTES) break;
    const char* nn = nv["n"] | (const char*)nullptr;
    int ms = nv["ms"] | 150;
    uint16_t f;
    if (!nn || !*nn) { dropped++; continue; }
    if (!noteToFreq(nn, f)) {
      // out of range (e.g. "C4", "A7") -> shift the octave into C5..B6
      char norm[5];
      bool ok = false;
      if (normalize(nn, norm, sizeof(norm))) {
        char& oct = norm[strlen(norm) - 1];
        if (oct < '5') oct = '5'; else if (oct > '6') oct = '6';
        ok = noteToFreq(norm, f);
      }
      if (!ok) { dropped++; continue; }
      fixed++;
    }
    if (ms < (int)NOTE_MIN_MS) { ms = NOTE_MIN_MS; fixed++; }
    if (ms > (int)NOTE_MAX_MS) { ms = NOTE_MAX_MS; fixed++; }
    tmp.notes[tmp.count++] = { f, (uint16_t)ms };
  }
  if (tmp.count < MELODY_MIN_NOTES)
    return fail("only " + String(tmp.count) + " usable notes (" + String(dropped) + " dropped), need " + String(MELODY_MIN_NOTES));
  if (dropped || fixed) Serial.printf("[melody] repaired '%s': %u notes kept, %u fixed, %u dropped\n", tmp.name, tmp.count, fixed, dropped);
  out = tmp;
  return true;
}

bool MelodyFormat::fromJson(const char* json, Melody& out, String* err) {
  JsonDocument doc;
  DeserializationError e = deserializeJson(doc, json);
  if (e) { if (err) *err = String("json: ") + e.c_str(); return false; }
  return fromJson(doc.as<JsonVariantConst>(), out, err);
}

String MelodyFormat::toJson(const Melody& m) {
  JsonDocument doc;
  doc["name"] = m.name;
  JsonArray arr = doc["notes"].to<JsonArray>();
  for (uint8_t i = 0; i < m.count; i++) {
    JsonObject n = arr.add<JsonObject>();
    n["n"] = freqToName(m.notes[i].freq);
    n["ms"] = m.notes[i].ms;
  }
  String s;
  serializeJson(doc, s);
  return s;
}

// ---- Player ----
Melody   MelodyPlayer::_m;
uint8_t  MelodyPlayer::_idx = 0;
uint32_t MelodyPlayer::_noteEnd = 0;
bool     MelodyPlayer::_playing = false;

void MelodyPlayer::load(const Melody& m) {
  bool was = _playing;
  stop();
  _m = m;
  if (was) play();
}

void MelodyPlayer::play(const Melody& m) {
  if (m.count == 0) return;
  _m = m;
  play();
}

void MelodyPlayer::play() {
  if (_m.count == 0) return;
  _idx = 0;
  _playing = true;
  _noteEnd = millis() + _m.notes[0].ms;
  Buzzer::tone(_m.notes[0].freq);
  Serial.printf("[melody] play '%s' (%u notes)\n", _m.name, _m.count);
}

void MelodyPlayer::stop() {
  if (_playing) Serial.println("[melody] stop");
  _playing = false;
  Buzzer::noTone();
}

bool MelodyPlayer::isPlaying() { return _playing; }
const char* MelodyPlayer::currentName() { return _m.name; }

void MelodyPlayer::update(uint32_t now) {
  if (!_playing) return;
  if ((int32_t)(now - _noteEnd) < 0) return;
  _idx = (_idx + 1) % _m.count;
  Buzzer::tone(_m.notes[_idx].freq);
  _noteEnd = now + _m.notes[_idx].ms;
}
