#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <SPIFFS.h>
#include <OV7670.h>
#include <JPEGENC.h>

// ========== WiFi AP ==========
const char* ssid     = "PipeBot";
const char* password = "12345678";

// ========== Camera with your NEW pin mapping ==========
OV7670 cam(OV7670::Mode::QQVGA_RGB565,
           // D0..D7
           39, 34, 14, 32, 33, 25, 26, 27,
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
  JPEGENCODE jpe;
  jpe.width  = cam.xres;
  jpe.height = cam.yres;
  jpe.bpp    = 16;
  jpe.format = JPEG_PIXEL_RGB565;
  jpe.pixels = (uint8_t*)img;
  jpg.open  = jpgOpen;
  jpg.close = jpgClose;
  jpg.write = jpgWrite;
  jpg.encode(&jpe, 50);
}

// ========== Web server & WebSocket ==========
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ========== WebSocket event (from phone) ==========
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_DATA) {
    String msg = String((char*)data);
    // Expected JSON: {"cmd":"move","x":...,"y":...}
    // Or {"cmd":"camera","x":...}, {"cmd":"led","id":1,"state":1}
    if (msg.indexOf("\"move\"") >= 0) {
      float x=0, y=0;
      int ix = msg.indexOf("\"x\":"); if (ix>=0) x = msg.substring(ix+4).toFloat();
      int iy = msg.indexOf("\"y\":"); if (iy>=0) y = msg.substring(iy+4).toFloat();
      // Differential mixing
      int left  = constrain(y*255 + x*255, -255, 255);
      int right = constrain(y*255 - x*255, -255, 255);
      Serial2.printf("M %d %d\n", left, right);  // send to Arduino
    }
    else if (msg.indexOf("\"camera\"") >= 0) {
      int ix = msg.indexOf("\"x\":"); if (ix>=0) {
        float x = msg.substring(ix+4).toFloat();  // -1..1
        int angle = map((int)(x*100), -100, 100, 0, 180);
        Serial2.printf("S %d\n", angle);
      }
    }
    else if (msg.indexOf("\"led\"") >= 0) {
      int id  = msg.substring(msg.indexOf("\"id\":")+5).toInt();
      int state = msg.substring(msg.indexOf("\"state\":")+8).toInt();
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
      // Parse "T:23.5 H:45.2 D:14.3 I:1"
      int tIdx = line.indexOf('T'); int hIdx = line.indexOf('H');
      int dIdx = line.indexOf('D'); int iIdx = line.indexOf('I');
      String temp = line.substring(tIdx+2, hIdx-1);
      String hum  = line.substring(hIdx+2, dIdx-1);
      String dist = line.substring(dIdx+2, iIdx-1);
      String ir   = line.substring(iIdx+2);
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

  // SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
  }

  // WiFi AP
  WiFi.softAP(ssid, password);
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());

  // Camera init
  if (cam.init()) {
    Serial.println("Camera OK");
  } else {
    Serial.println("Camera init failed – check wiring");
  }

  // WebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // Serve static files (the HTML page)
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  // MJPEG stream endpoint
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
          memcpy(buffer+headerLen, jpg_buf, jpg_len);
          return headerLen + jpg_len;
        }
        return 0;  // frame too big
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
