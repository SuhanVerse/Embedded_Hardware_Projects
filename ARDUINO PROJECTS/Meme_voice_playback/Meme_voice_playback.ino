#define FLAME_PIN 11     // Digital output from flame sensor (DO)
#define ISD_PIN   12     // ISD1820 P-E trigger pin

// Debounce / guard settings
const unsigned long triggerHoldMs = 50;    // Minimum time flame must persist
const unsigned long retriggerBlockMs = 3000; // Cooldown after playing
const unsigned long playPulseMs = 100;     // Edge trigger pulse for ISD1820

unsigned long lastPlayMs = 0;

void setup() {
  pinMode(FLAME_PIN, INPUT);    // If your module has pull-ups, keep INPUT; else use INPUT_PULLUP and invert logic
  pinMode(ISD_PIN, OUTPUT);
  digitalWrite(ISD_PIN, LOW);
  Serial.begin(9600);
  Serial.println("Flame Meme Player Ready...");
}

bool flameDetected() {
  // Adjust logic if your module outputs HIGH on detection
  int state = digitalRead(FLAME_PIN);
  return (state == LOW); // Common: LOW means flame detected
}

bool flameStable(unsigned long minMs) {
  unsigned long start = millis();
  while (millis() - start < minMs) {
    if (!flameDetected()) return false;
    delay(5);
  }
  return true;
}

void playMemeOnce() {
  digitalWrite(ISD_PIN, HIGH);
  delay(playPulseMs);
  digitalWrite(ISD_PIN, LOW);
  lastPlayMs = millis();
}

void loop() {
  unsigned long now = millis();

  // Block rapid retriggers
  if (now - lastPlayMs < retriggerBlockMs) {
    delay(20);
    return;
  }

  if (flameDetected() && flameStable(triggerHoldMs)) {
    Serial.println("Flame detected! Playing meme...");
    playMemeOnce();
  }

  delay(20); // Sampling interval
}
