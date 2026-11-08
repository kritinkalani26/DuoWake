#include <Adafruit_NeoPixel.h>

const int RING_PIN   = 27;
const int NUM_LEDS   = 12;
const int BUZZER_PIN = 25;

Adafruit_NeoPixel ring(NUM_LEDS, RING_PIN, NEO_GRB + NEO_KHZ800);

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  void buzzerOn(int freq)  { ledcWriteTone(BUZZER_PIN, freq); }
  void buzzerOff()         { ledcWriteTone(BUZZER_PIN, 0); }
  void buzzerSetup()       { ledcAttach(BUZZER_PIN, 2000, 10); }
#else
  const int CH = 0;
  void buzzerOn(int freq)  { ledcWriteTone(CH, freq); ledcWrite(CH, 512); }
  void buzzerOff()         { ledcWrite(CH, 0); }
  void buzzerSetup()       { ledcSetup(CH, 2000, 10); ledcAttachPin(BUZZER_PIN, CH); }
#endif

void fillRing(uint32_t color) {
  for (int i = 0; i < NUM_LEDS; i++) ring.setPixelColor(i, color);
  ring.show();
}

void setup() {
  Serial.begin(115200);
  delay(300);
  ring.begin();
  ring.setBrightness(30);
  ring.clear();
  ring.show();
  buzzerSetup();
  buzzerOff();
  Serial.println("\n=== ESP32 smoke test: ring on GPIO 27, buzzer on GPIO 25 ===");
}

void loop() {
  uint32_t colors[] = {
    ring.Color(255, 0, 0),
    ring.Color(0, 255, 0),
    ring.Color(0, 0, 255)
  };
  for (int i = 0; i < 3; i++) {
    Serial.printf("beep %d: ring on + buzzer on\n", i + 1);
    fillRing(colors[i]);
    buzzerOn(i == 2 ? 1500 : 1000);
    delay(200);
    fillRing(0);
    buzzerOff();
    delay(200);
  }
  Serial.println("pause 2 s");
  delay(2000);
}