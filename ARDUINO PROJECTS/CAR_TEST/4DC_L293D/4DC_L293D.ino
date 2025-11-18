// Motor A
int IN1 = 2;
int IN2 = 3;
// Motor B
int IN3 = 4;
int IN4 = 5;
// Motor C
int IN5 = 6;
int IN6 = 7;
// Motor D
int IN7 = 8;
int IN8 = 9;

void setup() {
  for (int pin = 2; pin <= 9; pin++) {
    pinMode(pin, OUTPUT);
  }
}

void loop() {
  // Motor A forward
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  // Motor B reverse
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  // Motor C forward
  digitalWrite(IN5, HIGH);
  digitalWrite(IN6, LOW);
  // Motor D reverse
  digitalWrite(IN7, LOW);
  digitalWrite(IN8, HIGH);

  delay(2000);

  // Stop all motors
  for (int pin = 2; pin <= 9; pin++) {
    digitalWrite(pin, LOW);
  }

  delay(2000);
}
