// Copy this file to firmware/include/secrets.h and fill in real values.
// secrets.h is gitignored.
#pragma once

#define WIFI_SSID      "YourPhoneHotspot"
#define WIFI_PASS      "hotspot-password"

#define GEMINI_API_KEY "AIza...your-key..."

// Find each board's MAC with:  pio run -e test_espnow -t upload && pio device monitor
// (it prints "MY MAC: xx:xx:xx:xx:xx:xx" on boot)
#define MASTER_MAC { 0x24, 0x6F, 0x28, 0x00, 0x00, 0x01 }
#define SLAVE_MAC  { 0x24, 0x6F, 0x28, 0x00, 0x00, 0x02 }
