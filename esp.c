/*
   ESP32-CAM (AI-Thinker) – DongiBaba WiFi
   Station mode with live camera viewer
   Board: "AI Thinker ESP32-CAM"
   Partition Scheme: "Huge APP"
*/

#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>

// ==================== CAMERA PINS (AI-Thinker) ====================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5

#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ==================== WiFi ====================
const char* WIFI_SSID = "DongiBaba";
const char* WIFI_PASS = "AbhI0703";

WebServer server(80);
bool cameraReady = false;

// ==================== MJPEG Stream ====================
void handleStream() {
  if (!cameraReady) {
    server.send(503, "text/plain", "Camera not available");
    return;
  }
  WiFiClient client = server.client();
  server.sendContent("HTTP/1.1 200 OK\r\n"
                     "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n");
  while (client.connected()) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Frame capture failed");
      delay(100);
      continue;
    }
    server.sendContent("--frame\r\n");
    server.sendContent("Content-Type: image/jpeg\r\n");
    server.sendContent("Content-Length: " + String(fb->len) + "\r\n\r\n");
    client.write(fb->buf, fb->len);
    server.sendContent("\r\n");
    esp_camera_fb_return(fb);
    if (!client.connected()) break;
    delay(30);
  }
}

// ==================== Snapshot ====================
void handleCapture() {
  if (!cameraReady) {
    server.send(503, "text/plain", "Camera not available");
    return;
  }
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "text/plain", "Capture failed");
    return;
  }
  server.sendHeader("Content-Type", "image/jpeg");
  server.sendHeader("Content-Length", String(fb->len));
  WiFiClient client = server.client();
  client.write(fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

// ==================== Root Page ====================
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 Camera Viewer</title>
  <style>
    body { font-family: sans-serif; text-align: center; background: #111; color: #eee; margin:0; padding:20px; }
    h2 { color: #f39c12; }
    img { max-width:100%; border:2px solid #444; border-radius:10px; }
    button { margin:10px; padding:10px 20px; font-size:16px; background:#444; color:white; border:none; border-radius:6px; cursor:pointer; }
    button:hover { background:#6272a4; }
    .ip { background:#222; padding:8px 20px; display:inline-block; border-radius:6px; margin-bottom:20px; }
    .error { color:#ff5555; font-weight:bold; }
  </style>
</head>
<body>
  <h2>ESP32 Camera Viewer</h2>
  <div class="ip">ESP IP: <span id="ip"></span></div>
  <div id="status"></div>
  <img id="stream" src="/stream" alt="Live Stream" onerror="this.style.display='none'; document.getElementById('status').innerHTML='<p class=error>Camera not connected or stream unavailable.</p>'">
  <br>
  <button onclick="location.href='/capture'">Take Snapshot</button>
  <script>
    document.getElementById('ip').textContent = window.location.hostname;
    var img = document.getElementById('stream');
    img.onload = function(){ document.getElementById('status').innerHTML = ''; };
  </script>
</body>
</html>
)rawliteral";
  server.send(200, "text/html", html);
}

// ==================== Camera Init ====================
bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk  = XCLK_GPIO_NUM;
  config.pin_pclk  = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href  = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn  = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size   = FRAMESIZE_VGA;
  config.jpeg_quality = 12;
  config.fb_count     = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }

  sensor_t * s = esp_camera_sensor_get();
  if (s) {
    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
    s->set_saturation(s, 0);
    s->set_special_effect(s, 0);
    s->set_whitebal(s, 1);
    s->set_awb_gain(s, 1);
    s->set_wb_mode(s, 0);
    s->set_exposure_ctrl(s, 1);
    s->set_aec2(s, 0);
    s->set_gain_ctrl(s, 1);
    s->set_agc_gain(s, 0);
    s->set_gainceiling(s, (gainceiling_t)0);
    s->set_bpc(s, 0);
    s->set_wpc(s, 1);
    s->set_raw_gma(s, 1);
    s->set_lenc(s, 1);
    s->set_hmirror(s, 0);
    s->set_vflip(s, 0);
    s->set_dcw(s, 1);
    s->set_colorbar(s, 0);
  }
  return true;
}

// ==================== Setup ====================
void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  if (!initCamera()) {
    Serial.println("Camera init failed – server will start anyway.");
  } else {
    Serial.println("Camera ready.");
    cameraReady = true;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("\nConnecting to ");
  Serial.println(WIFI_SSID);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  IPAddress ip = WiFi.localIP();
  Serial.println("\n=================================");
  Serial.println("WiFi Connected!");
  Serial.print("SSID: "); Serial.println(WIFI_SSID);
  Serial.print("IP Address: "); Serial.println(ip);
  Serial.println(cameraReady ? "Camera: READY" : "Camera: NOT AVAILABLE");
  Serial.println("Viewer: http://" + ip.toString() + "/");
  Serial.println("=================================");

  server.on("/", handleRoot);
  server.on("/stream", handleStream);
  server.on("/capture", handleCapture);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
}
