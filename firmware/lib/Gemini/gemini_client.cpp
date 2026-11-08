#include "gemini_client.h"
#include "alarm_config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

namespace {
  const char* _key = "";
  String _lastStatus = "idle";
  constexpr uint16_t HTTP_TIMEOUT_MS = 45000;
  const char* HOST = "https://generativelanguage.googleapis.com";

  // POST generateContent. On success fills modelText with the model's text part. Returns HTTP code (<0 = transport error).
  int generate(const String& prompt, JsonDocument& schema, float temperature, String& modelText, String& err) {
    if (WiFi.status() != WL_CONNECTED) { err = "wifi down"; return -1; }
    JsonDocument req;
    JsonArray contents = req["contents"].to<JsonArray>();
    JsonObject c = contents.add<JsonObject>();
    c["role"] = "user";
    c["parts"].to<JsonArray>().add<JsonObject>()["text"] = prompt;
    JsonObject gc = req["generationConfig"].to<JsonObject>();
    gc["responseMimeType"] = "application/json";
    gc["responseSchema"] = schema;
    gc["temperature"] = temperature;
    String body;
    serializeJson(req, body);

    String url = String(HOST) + "/v1beta/models/" + GEMINI_MODEL + ":generateContent?key=" + _key;
    WiFiClientSecure client;
    client.setInsecure();           // hackathon: skip cert validation
    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.setConnectTimeout(HTTP_TIMEOUT_MS);
    if (!http.begin(client, url)) { err = "http.begin failed"; return -2; }
    http.addHeader("Content-Type", "application/json");
    Serial.printf("[gemini] POST %s (%u bytes)\n", GEMINI_MODEL, body.length());
    uint32_t t0 = millis();
    int code = http.POST(body);
    String resp = http.getString();
    http.end();
    Serial.printf("[gemini] HTTP %d in %lu ms, %u bytes\n", code, (unsigned long)(millis() - t0), resp.length());

    if (code != 200) {
      err = "HTTP " + String(code) + ": " + resp.substring(0, 200);
      if (code == 404) { Serial.println("[gemini] 404 - model name probably wrong. Listing models:"); GeminiClient::listModels(); }
      return code;
    }
    JsonDocument doc;
    DeserializationError e = deserializeJson(doc, resp);
    if (e) { err = String("response json: ") + e.c_str(); return -3; }
    const char* text = doc["candidates"][0]["content"]["parts"][0]["text"] | (const char*)nullptr;
    if (!text) {
      const char* reason = doc["candidates"][0]["finishReason"] | doc["promptFeedback"]["blockReason"] | "no text";
      err = String("empty candidate: ") + reason;
      return -4;
    }
    modelText = text;
    Serial.printf("[gemini] model text: %s\n", modelText.substring(0, 160).c_str());
    return 200;
  }
}

void GeminiClient::begin(const char* apiKey) {
  _key = apiKey ? apiKey : "";
  Serial.printf("[gemini] model=%s key=%s\n", GEMINI_MODEL, strlen(_key) > 8 ? "set" : "MISSING");
}

String GeminiClient::lastStatus() { return _lastStatus; }

bool GeminiClient::generateMelody(const String& existingNames, Melody& out, String& err) {
  JsonDocument schema;
  deserializeJson(schema, R"({
    "type":"OBJECT",
    "properties":{
      "name":{"type":"STRING"},
      "notes":{"type":"ARRAY","items":{"type":"OBJECT","properties":{"n":{"type":"STRING","enum":["C5","C#5","D5","D#5","E5","F5","F#5","G5","G#5","A5","A#5","B5","C6","C#6","D6","D#6","E6","F6","F#6","G6","G#6","A6","A#6","B6","REST"]},"ms":{"type":"INTEGER"}},"required":["n","ms"]}}
    },
    "required":["name","notes"]})");

  String prompt =
    "You compose alarm-clock melodies for a tiny piezo buzzer. Write ONE short, energetic, slightly annoying "
    "wake-up melody of 8 to 16 notes. Rules: note names must be from C5 to B6 ONLY (two octaves: C5 C#5 D5 ... B5 C6 ... B6) using sharps, never flats, never octave 4 (e.g. C5, F#5, A#6) "
    "or REST; every duration 'ms' must be an integer between 50 and 2000 (mostly 80-300 for energy). "
    "Give it a fun, memorable 2-3 word name. Melodies that already exist (do NOT reuse these names, make something "
    "different in feel): " + (existingNames.length() ? existingNames : String("none")) + ". "
    "Return only JSON matching the schema.";

  for (uint8_t attempt = 1; attempt <= 2; attempt++) {
    String text;
    int code = generate(prompt, schema, 1.0f, text, err);
    if (code != 200) { _lastStatus = "melody failed: " + err; Serial.printf("[gemini] melody attempt %u: %s\n", attempt, err.c_str()); if (code == 404 || code == 400 || code == 403 || code == -1) return false; continue; }
    String verr;
    if (MelodyFormat::fromJson(text.c_str(), out, &verr)) {
      _lastStatus = String("melody ok: ") + out.name;
      Serial.printf("[gemini] melody '%s' valid (%u notes)\n", out.name, out.count);
      return true;
    }
    err = "invalid melody: " + verr;
    Serial.printf("[gemini] melody attempt %u invalid: %s\n", attempt, verr.c_str());
    prompt += " Your previous answer was rejected because: " + verr + ". Follow the rules exactly.";
  }
  _lastStatus = "melody failed: " + err;
  return false;
}

bool GeminiClient::pickGame(const String& historyJson, const String& gamesCsv, String& game, String& reason, String& err) {
  JsonDocument schema;
  schema["type"] = "OBJECT";
  JsonObject props = schema["properties"].to<JsonObject>();
  JsonObject g = props["game"].to<JsonObject>();
  g["type"] = "STRING";
  JsonArray en = g["enum"].to<JsonArray>();
  // split csv
  int start = 0;
  while (start <= (int)gamesCsv.length()) {
    int comma = gamesCsv.indexOf(',', start);
    if (comma < 0) comma = gamesCsv.length();
    String n = gamesCsv.substring(start, comma); n.trim();
    if (n.length()) en.add(n);
    start = comma + 1;
  }
  props["reason"]["type"] = "STRING";
  JsonArray req = schema["required"].to<JsonArray>();
  req.add("game"); req.add("reason");

  String prompt =
    "You are a mischievous alarm clock choosing tomorrow's wake-up challenge. Games: reaction (tap when a light "
    "flashes), rotate (tap when the spinning light hits the target), sprint (run between two clocks across the room). "
    "Pick ONE from [" + gamesCsv + "]. Avoid repeating the most recent game in this history if possible. "
    "History (oldest first): " + (historyJson.length() ? historyJson : String("[]")) + ". "
    "Return JSON with 'game' and a 'reason': one playful sentence (max 20 words) addressed to the sleeper.";

  String text;
  int code = generate(prompt, schema, 1.2f, text, err);
  if (code != 200) { _lastStatus = "game pick failed: " + err; return false; }
  JsonDocument doc;
  if (deserializeJson(doc, text)) { err = "pick json invalid"; _lastStatus = "game pick failed: " + err; return false; }
  String gname = doc["game"] | "";
  String r = doc["reason"] | "";
  // validate against csv
  bool ok = false;
  start = 0;
  while (start <= (int)gamesCsv.length()) {
    int comma = gamesCsv.indexOf(',', start);
    if (comma < 0) comma = gamesCsv.length();
    String n = gamesCsv.substring(start, comma); n.trim();
    if (n.length() && n == gname) ok = true;
    start = comma + 1;
  }
  if (!ok) { err = "pick returned unknown game '" + gname + "'"; _lastStatus = "game pick failed: " + err; return false; }
  game = gname;
  reason = r.length() ? r : String("Gemini picked it and kept the reason to itself.");
  _lastStatus = "game pick ok: " + game;
  Serial.printf("[gemini] picked '%s': %s\n", game.c_str(), reason.c_str());
  return true;
}

void GeminiClient::listModels() {
  if (WiFi.status() != WL_CONNECTED) { Serial.println("[gemini] listModels: wifi down"); return; }
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  String url = String(HOST) + "/v1beta/models?key=" + _key;
  if (!http.begin(client, url)) { Serial.println("[gemini] listModels begin failed"); return; }
  int code = http.GET();
  String resp = http.getString();
  http.end();
  Serial.printf("[gemini] GET /v1beta/models -> HTTP %d\n", code);
  if (code != 200) { Serial.println(resp.substring(0, 300)); return; }
  JsonDocument doc;
  JsonDocument filter;
  filter["models"][0]["name"] = true;
  filter["models"][0]["supportedGenerationMethods"] = true;
  if (deserializeJson(doc, resp, DeserializationOption::Filter(filter))) { Serial.println("[gemini] models json parse failed"); return; }
  for (JsonObject m : doc["models"].as<JsonArray>()) {
    bool gen = false;
    for (const char* meth : m["supportedGenerationMethods"].as<JsonArrayConst>()) if (strcmp(meth, "generateContent") == 0) gen = true;
    if (gen) Serial.printf("  %s\n", (const char*)m["name"]);
  }
  Serial.println("[gemini] set GEMINI_MODEL in lib/Hardware/config.h to one of the above (without 'models/')");
}
