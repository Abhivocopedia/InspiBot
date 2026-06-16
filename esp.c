#include <WiFi.h>
#include <WebServer.h>
#include "OV7670.h"   // use an OV7670 driver library

// ===== WiFi AP =====
const char* ssid = "PipeBot";
const char* password = "12345678";

WebServer server(80);
OV7670 camera;

// ===== MJPEG stream handler =====
void handleStream() {
  WiFiClient client = server.client();
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  client.print(response);

  while (client.connected()) {
    // Grab a frame from OV7670
    camera.captureFrame();
    uint8_t* buf = camera.getFrameBuffer();
    size_t len = camera.getFrameSize();

    client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %d\r\n\r\n", len);
    client.write(buf, len);
    client.print("\r\n");
    delay(50);
  }
}

void setup() {
  Serial.begin(115200);

  // Initialize camera
  if (!camera.begin(QQVGA, PIXFORMAT_JPEG)) {
    Serial.println("Camera init failed!");
    return;
  }
  Serial.println("Camera initialized.");

  // Start WiFi AP
  WiFi.softAP(ssid, password);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  // Routes
  server.on("/stream", [](){ handleStream(); });
  server.begin();
  Serial.println("Server started");
}

void loop() {
  server.handleClient();
}
