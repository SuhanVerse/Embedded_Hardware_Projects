#include <SoftwareSerial.h>

SoftwareSerial mySerial(2, 3); // RX, TX (HC-05)

#define enA 10  // L298N Enable A (Right)
#define in1 9   // Right motor IN1
#define in2 8   // Right motor IN2
#define in3 7   // Left motor IN3
#define in4 6   // Left motor IN4
#define enB 5   // L298N Enable B (Left)

int Speed = 220;
int command;

void setup() {
  Serial.begin(9600);
  mySerial.begin(9600);
  Serial.println("You're connected via Bluetooth");
  pinMode(enA, OUTPUT);
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  pinMode(in3, OUTPUT);
  pinMode(in4, OUTPUT);
  pinMode(enB, OUTPUT);

}

void loop() {
   if (mySerial.available())
   {
    analogWrite(enA, Speed);
    analogWrite(enB, Speed);
    
    command=(mySerial.read());
    if (command=='F')
    {
      Serial.println("Forward");
      digitalWrite(in1, LOW);  
      digitalWrite(in2, HIGH);
      digitalWrite(in3, LOW);  
      digitalWrite(in4, HIGH);          
    }
    
        else if (command=='B')
    {
      Serial.println("Reverse");
      digitalWrite(in1, HIGH); 
      digitalWrite(in2, LOW);
      digitalWrite(in3, HIGH); 
      digitalWrite(in4, LOW);
    }
    
        else if (command=='L')
    {
      Serial.println("Left");
      digitalWrite(in1, HIGH); 
      digitalWrite(in2, LOW);
      digitalWrite(in3, LOW);  
      digitalWrite(in4, HIGH);
    }

        else if (command=='R')
    {
      Serial.println("Right");
      digitalWrite(in1, LOW);  
      digitalWrite(in2, HIGH);
      digitalWrite(in3, HIGH); 
      digitalWrite(in4, LOW);
    }

        else if (command=='S')
    {
      Serial.println("Stop");
      digitalWrite(in1, LOW);  
      digitalWrite(in2, LOW);
      digitalWrite(in3, LOW);  
      digitalWrite(in4, LOW);

    }
   delay(10);
  }
}
