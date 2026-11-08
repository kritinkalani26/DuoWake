// Minimal button -> buzzer test. No libraries needed.
// Button: one leg to GPIO 33, other leg to GND.
// Buzzer: GPIO 25 -> 1k -> 2N3904 base (or buzzer directly on 25/GND for a quick test).
// Open Serial Monitor at 115200. It prints the raw pin state twice a second
// so you can see whether the button is actually changing the pin.

const int BUTTON_PIN = 33;
const int BUZZER_PIN = 25;
const int ONBOARD_LED = 2;   // small blue LED on most ESP32 dev boards

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  void buzzerSetup()      { ledcAttach(BUZZER_PIN, 2000, 10); }
  void buzzerOn(int freq) { ledcWriteTone(BUZZER_PIN, freq); }
  void buzzerOff()        { ledcWriteTone(BUZZER_PIN, 0); }
#else
  const int CH = 0;
  void buzzerSetup()      { ledcSetup(CH, 2000, 10); ledcAttachPin(BUZZER_PIN, CH); }
  void buzzerOn(int freq) { ledcWriteTone(CH, freq); ledcWrite(CH, 512); }
  void buzzerOff()        { ledcWrite(CH, 0); }
#endif

void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(ONBOARD_LED, OUTPUT);
  buzzerSetup();

  Serial.println();
  Serial.println("=== button -> buzzer test ===");
  Serial.println("Button on GPIO 33 (to GND), buzzer on GPIO 25");

  // Startup beep so you know the buzzer wiring works on its own
  buzzerOn(1000); delay(150); buzzerOff();
}

unsigned long lastPrint = 0;

void loop() {
  bool pressed = (digitalRead(BUTTON_PIN) == LOW);   // pull-up: LOW means pressed

  if (pressed) {
    buzzerOn(1500);
    digitalWrite(ONBOARD_LED, HIGH);
  } else {
    buzzerOff();
    digitalWrite(ONBOARD_LED, LOW);
  }

  if (millis() - lastPrint > 500) {
    lastPrint = millis();
    Serial.print("GPIO33 = ");
    Serial.print(pressed ? "LOW  (PRESSED)" : "HIGH (released)");
    Serial.println();
  }
}
