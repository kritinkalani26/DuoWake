// alarm_config.h - tunable, non-pin constants shared across modules.
#pragma once
#include <stdint.h>

// --- Sprint game ---
constexpr float    RUN_PACE_MPS = 2.5f;   // assumed running pace, meters/second
constexpr float    LEG_SLACK_S  = 2.0f;   // extra seconds per leg
constexpr uint8_t  SPRINT_LEGS  = 4;      // A->B->A->B->A

// --- Reaction game ---
constexpr uint8_t  REACTION_HITS_NEEDED   = 5;
constexpr uint16_t REACTION_WINDOW_START  = 600;
constexpr uint16_t REACTION_WINDOW_SHRINK = 50;
constexpr uint16_t REACTION_WINDOW_FLOOR  = 250;
constexpr uint16_t REACTION_GAP_MIN_MS    = 1000;
constexpr uint16_t REACTION_GAP_MAX_MS    = 4000;

// --- Rotate game ---
constexpr uint8_t  ROTATE_HITS_NEEDED = 5;
constexpr uint16_t ROTATE_STEP_START  = 120;
constexpr uint16_t ROTATE_STEP_DEC    = 15;
constexpr uint16_t ROTATE_STEP_FLOOR  = 50;
constexpr uint8_t  ROTATE_TARGET_IDX  = 0;

// --- Network ---
constexpr uint32_t WIFI_RETRY_MS     = 10000;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
constexpr const char* NTP_SERVER     = "pool.ntp.org";
constexpr const char* TZ_NEW_YORK    = "EST5EDT,M3.2.0,M11.1.0";  // America/New_York with DST
constexpr const char* MDNS_NAME      = "duowake";

// --- Gemini ---
constexpr const char* GEMINI_MODEL = "gemini-flash-lite-latest";
constexpr uint16_t MELODY_MIN_NOTES = 8;
constexpr uint16_t MELODY_MAX_NOTES = 16;
constexpr uint16_t NOTE_MIN_MS = 50;
constexpr uint16_t NOTE_MAX_MS = 2000;

// --- Melody store ---
constexpr const char* MELODY_DIR    = "/melodies";
constexpr const char* HISTORY_FILE  = "/history.json";

// --- Clock ids used in Game::onButton ---
constexpr uint8_t CLOCK_MASTER = 0;
constexpr uint8_t CLOCK_SLAVE  = 1;
