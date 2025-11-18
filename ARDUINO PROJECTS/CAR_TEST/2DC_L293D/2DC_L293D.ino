// Motor A
int IN1 = 2;
int IN2 = 3;
// Motor B
int IN3 = 4;
int IN4 = 5;

void setup() {
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
}

void loop() {
  // Motor A forward
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  // Motor B reverse
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);

  delay(2000);

  // Stop both motors
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  delay(2000);
}
