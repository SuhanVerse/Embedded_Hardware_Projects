const int touchPin = 2;     // Sensor I/O connected to D2
const int ledPin = 13;      // Built-in LED

bool lastState = LOW;

void setup() {
  pinMode(touchPin, INPUT);
  pinMode(ledPin, OUTPUT);
  Serial.begin(9600);
}

void loop() {
  bool currentState = digitalRead(touchPin);

  if (currentState == HIGH && lastState == LOW) {
    // Touch just occurred
    Serial.println("Touched!");
    digitalWrite(ledPin, HIGH);
  } else if (currentState == LOW && lastState == HIGH) {
    // Touch just released
    digitalWrite(ledPin, LOW);
  }

  lastState = currentState;
}
