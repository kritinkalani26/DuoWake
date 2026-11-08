// slave_app.h - SLAVE (clock B): dark and silent unless MASTER commands it over ESP-NOW.
// Joins the hotspot only to learn/lock the channel (re-locks on every reconnect).
#pragma once
#include <Arduino.h>

namespace SlaveApp {
  void begin();
  void loop();
}
