#include "esp_camera.h"
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>
#include <sstream>
#include "driver/ledc.h"

// ===================
// Select RHYX-M21 camera
// ===================
#define CAMERA_MODEL_RHYX_M21

// RHYX-M21 (GC2145) pin configuration
// Ensure this matches your camera_pins.h or include directly:
#define PWDN_GPIO_NUM  32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  0
#define SIOD_GPIO_NUM  26
#define SIOC_GPIO_NUM  27

#define Y9_GPIO_NUM    35
#define Y8_GPIO_NUM    34
#define Y7_GPIO_NUM    39
#define Y6_GPIO_NUM    36
#define Y5_GPIO_NUM    21
#define Y4_GPIO_NUM    19
#define Y3_GPIO_NUM    18
#define Y2_GPIO_NUM    5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM  23
#define PCLK_GPIO_NUM  22

// Flash LED pin (use 4 for flash LED or 33 for normal LED if your board supports)
#define LED_GPIO_NUM   4

// ===================
// Wi-Fi (AP mode for direct control)
// ===================
const char* ssid     = "Cipher";
const char* password = "echo1234";

// ===================
// Web server + websockets
// ===================
AsyncWebServer server(80);
AsyncWebSocket wsCamera("/Camera");
AsyncWebSocket wsServoInput("/ServoInput");
uint32_t cameraClientId = 0;

// ===================
// Servo pins and objects
// ===================
#define DUMMY_SERVO1_PIN 12
#define DUMMY_SERVO2_PIN 13
#define PAN_PIN 14
#define TILT_PIN 15

Servo dummyServo1, dummyServo2;
Servo panServo, tiltServo;

// ===================
// Light (PWM) control
// ===================
#define LIGHT_PIN LED_GPIO_NUM
const int PWMLightChannel = 4;

// ===================
// UI (modern dark theme with live indicators)
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
    #cameraImage { width:100%; max-width:500px; border-radius:8px; border:2px solid #333; margin-bottom:20px; }
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
    <img id="cameraImage" src="" alt="Camera Stream">

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
      <button class="btn" onclick="startStream()">&#9654; Start</button>
      <button class="btn" onclick="stopStream()">&#9632; Stop</button>
      <button class="btn" onclick="snapFrame()">&#128247; Snap</button>
      <button class="btn" onclick="calibrateServos()">&#128260; Calibrate</button>
    </div>
    <div id="status">Stream stopped</div>
  </div>

  <script>
    var webSocketCameraUrl = "ws://" + window.location.hostname + "/Camera";
    var webSocketServoInputUrl = "ws://" + window.location.hostname + "/ServoInput";
    var websocketCamera, websocketServoInput;

    function initCameraWebSocket() {
      websocketCamera = new WebSocket(webSocketCameraUrl);
      websocketCamera.binaryType = 'blob';
      websocketCamera.onmessage = function(event) {
        document.getElementById("cameraImage").src = URL.createObjectURL(event.data);
      };
      websocketCamera.onclose = () => {
        document.getElementById('status').innerText = 'Camera disconnected — attempting reconnect...';
        setTimeout(initCameraWebSocket, 2000);
      };
      websocketCamera.onopen = () => {
        document.getElementById('status').innerText = 'Streaming...';
      };
    }

    function initServoInputWebSocket() {
      websocketServoInput = new WebSocket(webSocketServoInputUrl);
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

    function startStream() {
      document.getElementById('status').innerText = 'Streaming...';
    }

    function stopStream() {
      document.getElementById('cameraImage').src = '';
      document.getElementById('status').innerText = 'Stream stopped';
    }

    function snapFrame() {
      const canvas = document.createElement('canvas');
      const img = document.getElementById('cameraImage');
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

    window.onload = () => { initCameraWebSocket(); initServoInputWebSocket(); };
  </script>
</body>
</html>
)HTMLHOMEPAGE";

// ===================
// Camera setup for GC2145 (RGB565 + frame2jpg)
// ===================
void setupCamera() {
  camera_config_t config;
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

  // GC2145 tuning
  config.xclk_freq_hz = 16000000;          // lower for stability on GC2145
  config.pixel_format = PIXFORMAT_RGB565;  // GC2145 outputs RGB565
  config.frame_size   = FRAMESIZE_QVGA;    // 320x240 for smoother FPS
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
}

// ===================
// Send JPEG over websocket
// ===================
void sendCameraPicture() {
  if (cameraClientId == 0) return;

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Frame buffer could not be acquired");
    return;
  }

  // Convert RGB565 to JPEG
  uint8_t *jpg_buf = NULL;
  size_t jpg_len = 0;
  bool converted = frame2jpg(fb, 80, &jpg_buf, &jpg_len); // quality 80
  if (converted && jpg_buf && jpg_len > 0) {
    wsCamera.binary(cameraClientId, jpg_buf, jpg_len);
    free(jpg_buf);
  }
  esp_camera_fb_return(fb);

  // Prevent queue overflow
  while (true) {
    AsyncWebSocketClient *clientPointer = wsCamera.client(cameraClientId);
    if (!clientPointer || !(clientPointer->queueIsFull())) break;
    delay(1);
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

  // LED PWM setup
  ledcSetup(PWMLightChannel, 1000, 8);   // channel, freq, resolution
  ledcAttachPin(LIGHT_PIN, PWMLightChannel);

  // Initial centers
  panServo.write(105); // Pan center
  tiltServo.write(0);  // Tilt center
  ledcWrite(PWMLightChannel, 0);
}


// ===================
// Websocket events (controls)
// ===================
void onServoInputWebSocketEvent(AsyncWebSocket *server,
                                AsyncWebSocketClient *client,
                                AwsEventType type,
                                void *arg,
                                uint8_t *data,
                                size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("Control client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("Control client #%u disconnected\n", client->id());
      // Auto-center servos and turn off light
      panServo.write(105);
      tiltServo.write(0);
      ledcWrite(PWMLightChannel, 0);
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
          // Map UI 0–180 around center 105 (±90)
          int servoVal = map(val, 0, 180, 105 - 90, 105 + 90);
          panServo.write(servoVal);
        } else if (key == "Tilt") {
          // Limit tilt to 0–115
          if (val < 0) val = 0;
          if (val > 115) val = 115;
          tiltServo.write(val);
        } else if (key == "Light") {
          if (val < 0) val = 0;
          if (val > 255) val = 255;
          ledcWrite(PWMLightChannel, val);
        } else if (key == "Calibrate") {
          panServo.write(105);
          tiltServo.write(0);
          ledcWrite(PWMLightChannel, 0);
        }
      }
      break;
    }
    default:
      break;
  }
}

// ===================
// Websocket events (camera)
// ===================
void onCameraWebSocketEvent(AsyncWebSocket *server,
                            AsyncWebSocketClient *client,
                            AwsEventType type,
                            void *arg,
                            uint8_t *data,
                            size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("Camera client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      cameraClientId = client->id();
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("Camera client #%u disconnected\n", client->id());
      cameraClientId = 0;
      // Optional: also auto-center on camera disconnect
      panServo.write(105);
      tiltServo.write(0);
      ledcWrite(PWMLightChannel, 0);
      break;
    default:
      break;
  }
}

// ===================
// HTTP handlers
// ===================
void handleRoot(AsyncWebServerRequest *request) {
  request->send_P(200, "text/html", htmlHomePage);
}

// ===================
// Setup + Loop
// ===================
void setup() {
  Serial.begin(115200);
  setUpPinModes();

  // AP mode for direct connection
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Routes
  server.on("/", HTTP_GET, handleRoot);

  // Websockets
  wsCamera.onEvent(onCameraWebSocketEvent);
  server.addHandler(&wsCamera);

  wsServoInput.onEvent(onServoInputWebSocketEvent);
  server.addHandler(&wsServoInput);

  server.begin();
  Serial.println("HTTP server started");

  setupCamera();
}

void loop() {
  wsCamera.cleanupClients();
  wsServoInput.cleanupClients();
  sendCameraPicture();
  // Small yield
  delay(5);
}
