// main.cpp - thin entry point. Role is chosen by the build flag (-DROLE_MASTER / -DROLE_SLAVE),
// see platformio.ini. All behaviour lives in lib/App and the module libraries.
#include <Arduino.h>

#if defined(ROLE_MASTER)
  #include "master_app.h"
  #define APP MasterApp
#elif defined(ROLE_SLAVE)
  #include "slave_app.h"
  #define APP SlaveApp
#else
  #error "Build with -DROLE_MASTER or -DROLE_SLAVE (use env:master or env:slave)"
#endif

void setup() {
  Serial.begin(115200);
  delay(200);
  APP::begin();
}

void loop() {
  APP::loop();
}
