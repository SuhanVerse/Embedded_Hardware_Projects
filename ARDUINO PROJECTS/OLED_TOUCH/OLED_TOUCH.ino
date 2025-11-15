#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const int touchPin = 2;  // Sensor I/O
bool lastState = LOW;
int animationMode = 0;

void setup() {
  pinMode(touchPin, INPUT);
  Serial.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED failed");
    while (true);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Touch to animate");
  display.display();
  delay(1000);
}

void loop() {
  bool currentState = digitalRead(touchPin);

  if (currentState == HIGH && lastState == LOW) {
    animationMode = (animationMode + 1) % 3;  // cycle through 3 modes
    runAnimation(animationMode);
  }

  lastState = currentState;
}

void runAnimation(int mode) {
  display.clearDisplay();

  switch (mode) {
    case 0:
      display.setCursor(0, 0);
      display.println("Animation 1");
      for (int i = 0; i < 128; i += 4) {
        display.fillCircle(i, 32, 3, WHITE);
        display.display();
        delay(30);
        display.clearDisplay();
      }
      break;

    case 1:
      display.setCursor(0, 0);
      display.println("Animation 2");
      for (int i = 0; i < 64; i += 4) {
        display.drawRect(0, i, 128, 64 - i, WHITE);
        display.display();
        delay(30);
      }
      break;

    case 2:
      display.setCursor(0, 0);
      display.println("Animation 3");
      for (int i = 0; i < 128; i += 8) {
        display.drawLine(0, 0, i, 64, WHITE);
        display.display();
        delay(30);
      }
      break;
  }

  delay(500);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Touch to animate");
  display.display();
}
