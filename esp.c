#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <SPIFFS.h>
#include <OV7670.h>
#include <JPEGENC.h>

// ========== WiFi AP ==========
const char* ssid     = "PipeBot";
const char* password = "12345678";

// ========== Camera with UPDATED pin mapping ==========
OV7670 cam(OV7670::Mode::QQVGA_RGB565,
           // D0..D7
           19, 34, 15, 32, 33, 25, 26, 27,
           // XCLK, PCLK, VSYNC, HREF, SIOD, SIOC
           4, 5, 13, 12, 21, 22);

JPEGENC jpg;
static uint8_t jpg_buf[8192];
int jpg_len = 0;

// JPEG callbacks
static void* jpgOpen(void) { return NULL; }
static void jpgClose(void* handle) {}
static int jpgWrite(JPEGENC* enc, void* handle, const uint8_t* data, int len) {
  if (jpg_len + len < sizeof(jpg_buf)) {
    memcpy(jpg_buf + jpg_len, data, len);
    jpg_len += len;
    return len;
  }
  return 0;
}

void captureAndCompress() {
  cam.oneFrame();
  uint16_t* img = cam.frame();
  jpg_len = 0;

  JPEGENC::CODEC jpe;
  jpe.width  = cam.xres;
  jpe.height = cam.yres;
  jpe.bpp    = 16;
  jpe.format = JPEG_PIXEL_RGB565;
  jpe.pixels = (uint8_t*)img;

  jpg.open  = jpgOpen;
  jpg.close = jpgClose;
  jpg.write = jpgWrite;

  jpg.encode(&jpe, 50);  // Quality = 50
}

// ========== Web server & WebSocket ==========
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ========== WebSocket event ==========
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_DATA) {
    String msg = String((char*)data).substring(0, len);

    if (msg.indexOf("\"move\"") >= 0) {
      float x = 0, y = 0;
      int ix = msg.indexOf("\"x\":"); if (ix >= 0) x = msg.substring(ix + 4).toFloat();
      int iy = msg.indexOf("\"y\":"); if (iy >= 0) y = msg.substring(iy + 4).toFloat();

      int left  = constrain((int)(y * 255 + x * 255), -255, 255);
      int right = constrain((int)(y * 255 - x * 255), -255, 255);
      Serial2.printf("M %d %d\n", left, right);
    }
    else if (msg.indexOf("\"camera\"") >= 0) {
      int ix = msg.indexOf("\"x\":");
      if (ix >= 0) {
        float x = msg.substring(ix + 4).toFloat();
        int angle = map((int)(x * 100), -100, 100, 0, 180);
        Serial2.printf("S %d\n", angle);
      }
    }
    else if (msg.indexOf("\"led\"") >= 0) {
      int idIdx = msg.indexOf("\"id\":");
      int stIdx = msg.indexOf("\"state\":");
      if (idIdx >= 0 && stIdx >= 0) {
        int id = msg.substring(idIdx + 5).toInt();
        int state = msg.substring(stIdx + 8).toInt();
        if (id == 1) Serial2.printf("L1 %d\n", state);
        if (id == 2) Serial2.printf("L2 %d\n", state);
      }
    }
  }
}

// ========== Read sensor data ==========
String lastSensorJSON = "{}";
unsigned long lastSensorRead = 0;

void readArduinoSensors() {
  if (Serial2.available()) {
    String line = Serial2.readStringUntil('\n');
    line.trim();
    if (line.startsWith("T:")) {
      int hIdx = line.indexOf('H');
      int dIdx = line.indexOf('D');
      int iIdx = line.indexOf('I');

      String temp = line.substring(2, hIdx).trim();
      String hum  = line.substring(hIdx + 2, dIdx).trim();
      String dist = line.substring(dIdx + 2, iIdx).trim();
      String ir   = line.substring(iIdx + 2).trim();

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

// ========== Setup ==========
void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17);  // RX2=16, TX2=17

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
  }

  WiFi.softAP(ssid, password);
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());

  if (cam.init()) {
    Serial.println("Camera OK");
  } else {
    Serial.println("Camera init failed – check wiring");
  }

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  server.on("/stream", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginChunkedResponse(
      "multipart/x-mixed-replace; boundary=frame",
      [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
        static char header[120];
        captureAndCompress();
        if (jpg_len == 0) return 0;

        int headerLen = snprintf(header, sizeof(header),
          "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %d\r\n\r\n", jpg_len);

        if (headerLen + jpg_len <= maxLen) {
          memcpy(buffer, header, headerLen);
          memcpy(buffer + headerLen, jpg_buf, jpg_len);
          return headerLen + jpg_len;
        }
        return 0;
      });
    request->send(response);
  });

  server.begin();
  Serial.println("Server started");
}

void loop() {
  ws.cleanupClients();
  readArduinoSensors();
  broadcastSensors();
  delay(5);
}
