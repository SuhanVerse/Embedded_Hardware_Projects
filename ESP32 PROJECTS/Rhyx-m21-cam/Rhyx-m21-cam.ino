#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>   // use this header for v1.x cores


// ===================
// Select camera model
// ===================
#define CAMERA_MODEL_RHYX_M21   // RHYX M21 camera with GC2145 sensor
#include "camera_pins.h"
// ===========================
// WiFi credentials
// ===========================
const char* ssid = "Your_ssid_name";
const char* password = "Your_ssid_password";

// Use ESP32WebServer instead of WebServer
WebServer server(80);

void handleStream() {
  WiFiClient client = server.client();
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  client.print(response);

  while (client.connected()) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      break;
    }

    // Convert RGB565 to JPEG in software
    size_t jpg_buf_len = 0;
    uint8_t *jpg_buf = NULL;
    bool converted = frame2jpg(fb, 80, &jpg_buf, &jpg_buf_len); // quality=80

    if (converted) {
      client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", jpg_buf_len);
      client.write(jpg_buf, jpg_buf_len);
      client.print("\r\n");
      free(jpg_buf);
    }

    esp_camera_fb_return(fb);
    delay(50); // ~20 fps cap
  }
}

void startMJPEGServer() {
server.on("/", HTTP_GET, []() {
  server.send(200, "text/html",
    "<!DOCTYPE html><html><head><title>ESP32-CAM GC2145</title><meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<style>"
    "body { margin:0; font-family:'Segoe UI',sans-serif; background:#121212; color:#eee; display:flex; flex-direction:column; align-items:center; }"
    ".card { background:#1e1e1e; padding:20px; margin-top:40px; border-radius:12px; box-shadow:0 0 20px rgba(0,0,0,0.5); width:95%; max-width:500px; text-align:center; }"
    "h2 { margin-bottom:10px; font-weight:500; }"
    "#stream { width:100%; max-width:480px; border-radius:8px; border:2px solid #333; display:block; margin-bottom:15px; }"
    ".btn { background:#03a9f4; color:white; border:none; padding:10px 20px; margin:5px; font-size:16px; border-radius:6px; cursor:pointer; transition:0.3s; }"
    ".btn:hover { background:#0288d1; }"
    "#status { margin-top:10px; font-size:14px; color:#ccc; }"
    "</style></head><body>"
    "<div class='card'>"
    "<h2>ESP32-CAM GC2145 Stream</h2>"
    "<img id='stream' src='' alt='Camera Stream'>"
    "<div>"
    "<button class='btn' onclick='startStream()'>&#9654; Start</button>"
    "<button class='btn' onclick='stopStream()'>&#9632; Stop</button>"
    "<button class='btn' onclick='snapFrame()'>&#128247; Snap</button>"
    "</div>"
    "<div id='status'>Stream stopped</div>"
    "</div>"
    "<script>"
    "function startStream() { document.getElementById('stream').src = '/stream'; document.getElementById('status').innerText = 'Streaming...'; }"
    "function stopStream() { document.getElementById('stream').src = ''; document.getElementById('status').innerText = 'Stream stopped'; }"
    "function snapFrame() {"
      "const canvas = document.createElement('canvas');"
      "const img = document.getElementById('stream');"
      "canvas.width = img.width; canvas.height = img.height;"
      "const ctx = canvas.getContext('2d');"
      "ctx.drawImage(img, 0, 0, canvas.width, canvas.height);"
      "canvas.toBlob(function(blob) {"
        "const link = document.createElement('a');"
        "link.href = URL.createObjectURL(blob);"
        "link.download = 'snapshot.jpg';"
        "link.click();"
      "}, 'image/jpeg');"
    "}"
    "</script>"
    "</body></html>");
});


  server.on("/stream", HTTP_GET, handleStream);
  server.begin();
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);

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

  // GC2145 tuning
  config.xclk_freq_hz = 16000000;          // lower for stability
  config.pixel_format = PIXFORMAT_RGB565;  // GC2145 has no JPEG
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
  Serial.printf("Camera sensor ID: 0x%x\n", s->id.PID);
  if (s->id.PID == 0x2145) {
    Serial.println("Detected GC2145 sensor (RHYX M21)");
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);
  }

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  startMJPEGServer();
  Serial.print("Camera Ready! Connect: http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  server.handleClient();
}

