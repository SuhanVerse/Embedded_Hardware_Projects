#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>          // synchronous server for MJPEG stream
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>  // async server for servo/light control
#include <ESP32Servo.h>
#include <sstream>
#include "driver/ledc.h"

#define CAMERA_MODEL_RHYX_M21
#include "camera_pins.h"

// Wi-Fi AP credentials
const char* ssid = "Cipher";
const char* password = "echo1234";

// Servers
WebServer mjpegServer(80);          // MJPEG stream
AsyncWebServer controlServer(80);   // same port, async routes
AsyncWebSocket wsServo("/ServoInput");

// Servo pins
#define DUMMY_SERVO1_PIN 12
#define DUMMY_SERVO2_PIN 13
#define PAN_PIN 14
#define TILT_PIN 15

Servo dummyServo1, dummyServo2;
Servo panServo, tiltServo;

// Light pin
#define LIGHT_PIN 4

// ===================
// HTML UI
// ===================
const char* htmlHomePage PROGMEM = R"HTMLHOMEPAGE(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32-CAM Pan Tilt</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { margin:0; font-family:'Segoe UI',sans-serif; background:#121212; color:#eee;
           display:flex; flex-direction:column; align-items:center; }
    .card { background:#1e1e1e; padding:20px; margin-top:30px; border-radius:12px;
            box-shadow:0 0 20px rgba(0,0,0,0.5); width:95%; max-width:520px; text-align:center; }
    h2 { margin-bottom:15px; font-weight:500; }
    #stream { width:100%; max-width:500px; border-radius:8px; border:2px solid #333; margin-bottom:20px; }
    .slider { -webkit-appearance:none; width:100%; height:20px; border-radius:10px; background:#333; outline:none; margin:10px 0; }
    .slider::-webkit-slider-thumb { -webkit-appearance:none; width:24px; height:24px; border-radius:50%; background:#03a9f4; cursor:pointer; }
    .slider::-moz-range-thumb { width:24px; height:24px; border-radius:50%; background:#03a9f4; cursor:pointer; }
    .row { text-align:left; margin-top:10px; font-size:14px; display:flex; justify-content:space-between; align-items:center; }
    .btn { background:#03a9f4; color:white; border:none; padding:10px 20px; margin:5px; font-size:16px; border-radius:6px; cursor:pointer; transition:0.3s; }
    .btn:hover { background:#0288d1; }
    #status { margin-top:10px; font-size:14px; color:#ccc; }
  </style>
</head>
<body>
  <div class="card">
    <h2>ESP32-CAM Pan Tilt Control</h2>
    <img id="stream" src="/stream" alt="Camera Stream">

    <div class="row"><b>Pan:</b><span id="panVal">90</span>°</div>
    <input type="range" min="0" max="180" value="90" class="slider" id="Pan"
           oninput='updateVal("Pan",value); sendButtonInput("Pan",value)'>

    <div class="row"><b>Tilt:</b><span id="tiltVal">0</span>°</div>
    <input type="range" min="0" max="115" value="0" class="slider" id="Tilt"
           oninput='updateVal("Tilt",value); sendButtonInput("Tilt",value)'>

    <div class="row"><b>Light:</b><span id="lightVal">0</span></div>
    <input type="range" min="0" max="255" value="0" class="slider" id="Light"
           oninput='updateVal("Light",value); sendButtonInput("Light",value)'>

    <div style="margin-top:15px;">
      <button class="btn" onclick="snapFrame()">📸 Snap</button>
      <button class="btn" onclick="calibrateServos()">🔄 Calibrate</button>
    </div>
    <div id="status">Ready</div>
  </div>

  <script>
    var websocketServoInput;
    function initServoInputWebSocket() {
      websocketServoInput = new WebSocket("ws://" + window.location.hostname + "/ServoInput");
      websocketServoInput.onopen = () => {
        sendButtonInput("Pan", document.getElementById("Pan").value);
        sendButtonInput("Tilt", document.getElementById("Tilt").value);
        sendButtonInput("Light", document.getElementById("Light").value);
        document.getElementById('status').innerText = 'Controls connected';
      };
      websocketServoInput.onclose = () => {
        document.getElementById('status').innerText = 'Controls disconnected — servos auto-centered';
        setTimeout(initServoInputWebSocket, 2000);
      };
    }
    function sendButtonInput(key, value) {
      if (websocketServoInput && websocketServoInput.readyState === 1) {
        websocketServoInput.send(key + "," + value);
      }
    }
    function updateVal(id, val) {
      document.getElementById(id.toLowerCase() + "Val").innerText = val;
    }
    function snapFrame() {
      const canvas = document.createElement('canvas');
      const img = document.getElementById('stream');
      canvas.width = img.width; canvas.height = img.height;
      const ctx = canvas.getContext('2d');
      ctx.drawImage(img, 0, 0, canvas.width, canvas.height);
      canvas.toBlob(function(blob) {
        const link = document.createElement('a');
        link.href = URL.createObjectURL(blob);
        link.download = 'snapshot.jpg';
        link.click();
      }, 'image/jpeg');
    }
    function calibrateServos() {
      sendButtonInput("Calibrate", 0);
      document.getElementById('panVal').innerText = "105";
      document.getElementById('tiltVal').innerText = "0";
      document.getElementById('lightVal').innerText = "0";
      document.getElementById('status').innerText = "Servos reset to center";
    }
    window.onload = () => { initServoInputWebSocket(); };
  </script>
</body>
</html>
)HTMLHOMEPAGE";

// ===================
// MJPEG stream handler (WebServer)
// ===================
void handleStream() {
  WiFiClient client = mjpegServer.client();
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  client.print(response);

  while (client.connected()) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      break;
    }
    size_t jpg_buf_len = 0;
    uint8_t *jpg_buf = NULL;
    bool converted = frame2jpg(fb, 80, &jpg_buf, &jpg_buf_len);
    if (converted) {
      client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", jpg_buf_len);
      client.write(jpg_buf, jpg_buf_len);
      client.print("\r\n");
      free(jpg_buf);
    }
    esp_camera_fb_return(fb);
    delay(50);
  }
}

// ===================
// Servo + Light setup
// ===================
void setUpPinModes() {
  dummyServo1.attach(DUMMY_SERVO1_PIN);
  dummyServo2.attach(DUMMY_SERVO2_PIN);
  panServo.attach(PAN_PIN);
  tiltServo.attach(TILT_PIN);
  ledcAttach(LIGHT_PIN, 1000, 8);
  panServo.write(105);
  tiltServo.write(0);
  ledcWrite(LIGHT_PIN, 0);
}

// ===================
// Servo websocket events
// ===================
void onServoInputWebSocketEvent(AsyncWebSocket *server,
                                AsyncWebSocketClient *client,
                                AwsEventType type,
                                void *arg,
                                uint8_t *data,
                                size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("Servo client #%u connected\n", client->id());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("Servo client #%u disconnected\n", client->id());
      panServo.write(105);
      tiltServo.write(0);
      ledcWrite(LIGHT_PIN, 0);
      break;
    case WS_EVT_DATA: {
      AwsFrameInfo *info = (AwsFrameInfo*)arg;
      if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        std::string msg((char*)data, len);
        std::istringstream ss(msg);
        std::string key, value;
        std::getline(ss, key, ',');
        std::getline(ss, value, ',');
        int val = value.empty() ? 0 : atoi(value.c_str());

        if (key == "Pan") {
          int servoVal = map(val, 0, 180, 105 - 90, 105 + 90);
          panServo.write(servoVal);
        } else if (key == "Tilt") {
          if (val < 0) val = 0;
          if (val > 115) val = 115;
          tiltServo.write(val);
        } else if (key == "Light") {
          if (val < 0) val = 0;
          if (val > 255) val = 255;
          ledcWrite(LIGHT_PIN, val);
        } else if (key == "Calibrate") {
          panServo.write(105);
          tiltServo.write(0);
          ledcWrite(LIGHT_PIN, 0);
        }
      }
      break;
    }
    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  setUpPinModes();

  // Camera config
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;

  config.xclk_freq_hz = 16000000;
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size   = FRAMESIZE_QVGA;
  config.fb_count     = psramFound() ? 2 : 1;
  config.grab_mode    = CAMERA_GRAB_LATEST;
  config.fb_location  = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;

  Serial.println("Initializing camera...");
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s && s->id.PID == 0x2145) {
    Serial.println("Detected GC2145 sensor (RHYX-M21)");
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);
  }

  // Wi-Fi AP mode
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // MJPEG stream route (WebServer)
  mjpegServer.on("/stream", HTTP_GET, handleStream);
  mjpegServer.on("/", HTTP_GET, []() {
    mjpegServer.send(200, "text/html", htmlHomePage);
  });
  mjpegServer.begin();

  // Servo websocket (Async)
  wsServo.onEvent(onServoInputWebSocketEvent);
  controlServer.addHandler(&wsServo);
  controlServer.begin();

  Serial.println("Unified server started on port 80");
}

void loop() {
  mjpegServer.handleClient();   // handle MJPEG stream
  wsServo.cleanupClients();     // cleanup servo websocket clients
}
