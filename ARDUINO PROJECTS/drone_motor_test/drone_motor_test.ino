#include <Servo.h>

Servo ESC;
int pot = A0;
int throttle;

void setup() {
  Serial.begin(9600);
  ESC.attach(6, 1000, 2000);  // proper ESC range

  ESC.write(0);               // send 1000 µs = minimum throttle
  delay(2000);                // wait for ESC to arm
}

void loop() {
  throttle = analogRead(pot);          // 0–1023
  throttle = map(throttle, 0, 1023, 0, 180);  
  ESC.write(throttle);                 // send mapped throttle
  Serial.println(throttle);
  delay(20);
}
