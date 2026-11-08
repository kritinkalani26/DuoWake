// builtin_tunes.h - 3 original, deliberately annoying alarm tunes in the melody JSON format.
#pragma once
#include <Arduino.h>

struct BuiltinTune { const char* id; const char* json; };

static const BuiltinTune BUILTIN_TUNES[] = {
  { "siren_stairs",
    "{\"name\":\"Siren Stairs\",\"notes\":["
    "{\"n\":\"C5\",\"ms\":120},{\"n\":\"E5\",\"ms\":120},{\"n\":\"G5\",\"ms\":120},{\"n\":\"C6\",\"ms\":200},"
    "{\"n\":\"REST\",\"ms\":60},{\"n\":\"B5\",\"ms\":100},{\"n\":\"G5\",\"ms\":100},{\"n\":\"E5\",\"ms\":100},"
    "{\"n\":\"C5\",\"ms\":180},{\"n\":\"REST\",\"ms\":80},{\"n\":\"C6\",\"ms\":90},{\"n\":\"C6\",\"ms\":90},"
    "{\"n\":\"REST\",\"ms\":200}]}" },
  { "wasp_nest",
    "{\"name\":\"Wasp Nest\",\"notes\":["
    "{\"n\":\"F#5\",\"ms\":80},{\"n\":\"G5\",\"ms\":80},{\"n\":\"F#5\",\"ms\":80},{\"n\":\"G5\",\"ms\":80},"
    "{\"n\":\"A#5\",\"ms\":160},{\"n\":\"REST\",\"ms\":50},{\"n\":\"F#5\",\"ms\":80},{\"n\":\"G5\",\"ms\":80},"
    "{\"n\":\"F#5\",\"ms\":80},{\"n\":\"G5\",\"ms\":80},{\"n\":\"D6\",\"ms\":220},{\"n\":\"REST\",\"ms\":120}]}" },
  { "hiccup_march",
    "{\"name\":\"Hiccup March\",\"notes\":["
    "{\"n\":\"A5\",\"ms\":100},{\"n\":\"REST\",\"ms\":50},{\"n\":\"A5\",\"ms\":100},{\"n\":\"REST\",\"ms\":50},"
    "{\"n\":\"E6\",\"ms\":260},{\"n\":\"REST\",\"ms\":90},{\"n\":\"D6\",\"ms\":100},{\"n\":\"C#6\",\"ms\":100},"
    "{\"n\":\"B5\",\"ms\":100},{\"n\":\"A5\",\"ms\":100},{\"n\":\"G#5\",\"ms\":260},{\"n\":\"REST\",\"ms\":50},"
    "{\"n\":\"A5\",\"ms\":100},{\"n\":\"A5\",\"ms\":100},{\"n\":\"REST\",\"ms\":300}]}" },
};
static const uint8_t BUILTIN_TUNE_COUNT = sizeof(BUILTIN_TUNES) / sizeof(BUILTIN_TUNES[0]);
