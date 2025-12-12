#include <WiFi.h>
#include <ESP32Servo.h>

// Servo objects
Servo panServo;
Servo tiltServo;

// GPIO pins for servos
static const int panPin  = 13;   // Pan servo pin
static const int tiltPin = 12;   // Tilt servo pin

// Wi-Fi credentials
const char* ssid     = "Cipher";
const char* password = "echo1234";

// Web server
WiFiServer server(80);

// HTTP request buffer
String header;

// Timeout control
unsigned long currentTime = millis();
unsigned long previousTime = 0;
const long timeoutTime = 2000;

void setup() {
  Serial.begin(115200);

  // Attach servos
  panServo.attach(panPin);
  tiltServo.attach(tiltPin);

  // Center servos on boot (after power reconnect)
  panServo.write(105);   // Pan neutral
  tiltServo.write(5);    // Tilt neutral
  delay(500);            // Small delay to let them settle

  // Connect Wi-Fi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.begin();
}


void loop() {
  WiFiClient client = server.available();

  if (client) {
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client.");
    String currentLine = "";

    while (client.connected() && currentTime - previousTime <= timeoutTime) {
      currentTime = millis();
      if (client.available()) {
        char c = client.read();
        header += c;

        if (c == '\n') {
          if (currentLine.length() == 0) {
            // Send HTTP response
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            // HTML + CSS + JS
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<style>");
            client.println("body { font-family: 'Segoe UI', sans-serif; background-color: #121212; color: #e0e0e0; text-align: center; padding: 20px; }");
            client.println("h1 { color: #00bcd4; margin-bottom: 30px; }");
            client.println(".slider { width: 80%; max-width: 400px; margin: 20px auto; }");
            client.println(".label { font-weight: bold; margin-top: 20px; font-size: 18px; }");
            client.println(".pos { color: #ff9800; font-size: 20px; }");
            client.println("input[type=range]::-webkit-slider-thumb { background: #00bcd4; }");
            client.println("</style>");
            client.println("<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/3.3.1/jquery.min.js\"></script>");
            client.println("</head><body>");
            client.println("<h1>ESP32 Pan&#8211;Tilt Control</h1>");

            client.println("<div class=\"label\">Pan (center=105): <span id=\"panPos\" class=\"pos\">105</span>&deg;</div>");
            client.println("<input type=\"range\" min=\"0\" max=\"180\" value=\"105\" class=\"slider\" id=\"panSlider\" onchange=\"servo('pan',this.value)\">");

            client.println("<div class=\"label\">Tilt (0&#8211;115): <span id=\"tiltPos\" class=\"pos\">0</span>&deg;</div>");
            client.println("<input type=\"range\" min=\"0\" max=\"115\" value=\"0\" class=\"slider\" id=\"tiltSlider\" onchange=\"servo('tilt',this.value)\">");

            client.println("<script>");
            client.println("function servo(axis,pos){ $.get('/?axis='+axis+'&value='+pos+'&'); }");
            client.println("var panSlider=document.getElementById('panSlider');");
            client.println("var tiltSlider=document.getElementById('tiltSlider');");
            client.println("var panPos=document.getElementById('panPos');");
            client.println("var tiltPos=document.getElementById('tiltPos');");
            client.println("panSlider.oninput=function(){ panPos.innerHTML=this.value; }");
            client.println("tiltSlider.oninput=function(){ tiltPos.innerHTML=this.value; }");
            client.println("</script>");
            client.println("</body></html>");

            // Parse GET request
            if (header.indexOf("GET /?axis=") >= 0) {
              int axisPos = header.indexOf("axis=");
              int valPos  = header.indexOf("value=");
              int ampPos  = header.indexOf('&', valPos);

              String axis = header.substring(axisPos+5, header.indexOf('&', axisPos));
              String valueString = header.substring(valPos+6, ampPos);
              int angle = valueString.toInt();

              if (axis == "pan") {
                // Pan servo: full 0–180 range, centered at 105
                panServo.write(angle);
                Serial.println("Pan: " + String(angle));
              } else if (axis == "tilt") {
                // Tilt servo: limit to 0–115
                if (angle < 0) angle = 0;
                if (angle > 115) angle = 115;
                tiltServo.write(angle);
                Serial.println("Tilt: " + String(angle));
              }
            }

            client.println();
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    header = "";
    client.stop();
    Serial.println("Client disconnected.\n");
  }
}
