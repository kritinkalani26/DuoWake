// secrets_loader.h - include the real secrets.h if present, else fall back to the example so builds still compile.
#pragma once
#if __has_include("secrets.h")
  #include "secrets.h"
#else
  #warning "include/secrets.h not found - using secrets.example.h placeholders. Copy it and fill in real values!"
  #include "secrets.example.h"
#endif

static const uint8_t MASTER_MAC_BYTES[6] = MASTER_MAC;
static const uint8_t SLAVE_MAC_BYTES[6]  = SLAVE_MAC;
