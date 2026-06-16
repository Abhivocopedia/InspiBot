#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <SPIFFS.h>
#include "esp_camera.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// ========== WiFi AP ==========
const char* ssid     = "PipeBot";
const char* password = "12345678";

// ========== CAMERA PIN DEFINITIONS (Your 30-pin ESP32) ==========
#define PWDN_GPIO_NUM     -1  // Not used
#define RESET_GPIO_NUM    -1  // Not used
#define XCLK_GPIO_NUM     4
#define SIOD_GPIO_NUM     21
#define SIOC_GPIO_NUM     22

#define Y9_GPIO_NUM       27  // D7
#define Y8_GPIO_NUM       26  // D6
#define Y7_GPIO_NUM       25  // D5
#define Y6_GPIO_NUM       33  // D4
#define Y5_GPIO_NUM       32  // D3
#define Y4_GPIO_NUM       15  // D2 (CHANGED from 14)
#define Y3_GPIO_NUM       34  // D1 (Input-only)
#define Y2_GPIO_NUM       19  // D0 (CHANGED from 39)

#define VSYNC_GPIO_NUM    13
#define HREF_GPIO_NUM     12
#define PCLK_GPIO_NUM     5

// ========== Camera Frame Buffer ==========
camera_fb_t * fb = NULL;

// ========== Web server & WebSocket ==========
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ========== WebSocket event handler ==========
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_DATA) {
    String msg = String((char*)data);
    
    // Move command: {"cmd":"move","x":...,"y":...}
    if (msg.indexOf("\"move\"") >= 0) {
      float x = 0, y = 0;
      int ix = msg.indexOf("\"x\":"); 
      if (ix >= 0) x = msg.substring(ix + 4).toFloat();
      int iy = msg.indexOf("\"y\":"); 
      if (iy >= 0) y = msg.substring(iy + 4).toFloat();
      
      int left  = constrain(y * 255 + x * 255, -255, 255);
      int right = constrain(y * 255 - x * 255, -255, 255);
      Serial2.printf("M %d %d\n", left, right);
    }
    // Camera command: {"cmd":"camera","x":...}
    else if (msg.indexOf("\"camera\"") >= 0) {
      int ix = msg.indexOf("\"x\":");
      if (ix >= 0) {
        float x = msg.substring(ix + 4).toFloat();
        int angle = map((int)(x * 100), -100, 100, 0, 180);
        Serial2.printf("S %d\n", angle);
      }
    }
    // LED command: {"cmd":"led","id":1,"state":1}
    else if (msg.indexOf("\"led\"") >= 0) {
      int id = msg.substring(msg.indexOf("\"id\":") + 5).toInt();
      int state = msg.substring(msg.indexOf("\"state\":") + 8).toInt();
      if (id == 1) Serial2.printf("L1 %d\n", state);
      if (id == 2) Serial2.printf("L2 %d\n", state);
    }
  }
}

// ========== Read sensor data from Arduino ==========
String lastSensorJSON = "{}";
unsigned long lastSensorRead = 0;

void readArduinoSensors() {
  if (Serial2.available()) {
    String line = Serial2.readStringUntil('\n');
    line.trim();
    if (line.startsWith("T:")) {
      int tIdx = line.indexOf('T'); 
      int hIdx = line.indexOf('H');
      int dIdx = line.indexOf('D'); 
      int iIdx = line.indexOf('I');
      String temp = line.substring(tIdx + 2, hIdx - 1);
      String hum  = line.substring(hIdx + 2, dIdx - 1);
      String dist = line.substring(dIdx + 2, iIdx - 1);
      String ir   = line.substring(iIdx + 2);
      lastSensorJSON = "{\"temp\":" + temp + ",\"hum\":" + hum +
                       ",\"dist\":" + dist + ",\"ir\":" + ir + "}";
    }
  }
}

void broadcastSensors() {
  if (millis() - lastSensorRead > 200) {
    lastSensorRead = millis();
    if (lastSensorJSON != "{}") {
      ws.textAll(lastSensorJSON);
    }
  }
}

// ========== MJPEG Stream Handler ==========
String getStreamResponse() {
  // Capture image
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return "";
  }
  
  // Convert to base64 or return raw
  // For streaming, we use chunked response directly
  
  // Return image data as base64 (simplified)
  return "";
}

// ========== Setup ==========
void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17);  // RX2=16, TX2=17

  // Disable brownout detector
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  // SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
  }

  // ====== Camera Configuration ======
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;  // Use JPEG for streaming
  config.frame_size = FRAMESIZE_QVGA;     // 320x240
  config.jpeg_quality = 12;               // 0-63 lower = better
  config.fb_count = 1;

  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    Serial.println(" - Check wiring!");
    return;
  }
  Serial.println("Camera initialized successfully!");

  // WiFi AP
  WiFi.softAP(ssid, password);
  Serial.print("AP IP: "); 
  Serial.println(WiFi.softAPIP());

  // WebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // Serve static files (HTML page)
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  // ====== MJPEG Stream Endpoint ======
  server.on("/stream", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginChunkedResponse(
      "multipart/x-mixed-replace; boundary=frame",
      [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
        static char header[120];
        
        // Capture frame from camera
        camera_fb_t * fb = esp_camera_fb_get();
        if (!fb) {
          return 0;
        }
        
        // Build header
        int headerLen = snprintf(header, sizeof(header),
          "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %d\r\n\r\n", 
          fb->len);
        
        // Check if buffer can fit header + image
        if (headerLen + fb->len <= maxLen) {
          memcpy(buffer, header, headerLen);
          memcpy(buffer + headerLen, fb->buf, fb->len);
          esp_camera_fb_return(fb);
          return headerLen + fb->len;
        }
        
        esp_camera_fb_return(fb);
        return 0;
      });
    request->send(response);
  });

  // ====== Simple Capture Endpoint ======
  server.on("/capture", HTTP_GET, [](AsyncWebServerRequest *request) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      request->send(500, "text/plain", "Camera capture failed");
      return;
    }
    
    AsyncWebServerResponse *response = request->beginResponse(200, "image/jpeg", fb->buf, fb->len);
    request->send(response);
    esp_camera_fb_return(fb);
  });

  // ====== Sensor Data Endpoint ======
  server.on("/sensors", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", lastSensorJSON);
  });

  server.begin();
  Serial.println("Server started");
  Serial.println("Connect to WiFi: PipeBot");
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  ws.cleanupClients();
  readArduinoSensors();
  broadcastSensors();
  delay(5);
}
