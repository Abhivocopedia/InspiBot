#include <WiFi.h>
#include <WebServer.h>
#include "OV7670.h"

const char* ssid = "PipeBot";
const char* password = "12345678";

WebServer server(80);
OV7670 camera;

void handleStream() {
  WiFiClient client = server.client();
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
  client.println();

  while (client.connected()) {
    camera.captureFrame();
    uint8_t* buf = camera.frameBuffer;
    size_t len = camera.frameSize;

    client.printf("--frame\r\nContent-Type: image/bmp\r\nContent-Length: %d\r\n\r\n", len);
    client.write(buf, len);
    client.print("\r\n");
    delay(100);
  }
}

void setup() {
  Serial.begin(115200);

  if (!camera.begin(OV7670::Resolution::QVGA, OV7670::PixelFormat::RGB565)) {
    Serial.println("Camera init failed!");
    return;
  }

  WiFi.softAP(ssid, password);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/stream", [](){ handleStream(); });
  server.begin();
}

void loop() {
  server.handleClient();
}
