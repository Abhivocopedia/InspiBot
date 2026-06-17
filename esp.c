/*
   ESP32 Camera AP Streaming Server
   Compatible with ESP32-WROOM-DA
   Arduino IDE 1.8.19 + ESP32 Core

   Features:
   - Camera initialization using manual pin mapping
   - WiFi Access Point mode
   - MJPEG stream at:
       http://192.168.4.1/stream
   - Camera snapshot:
       http://192.168.4.1/capture
   - LED control:
       /led1/on
       /led1/off
   - Motor control:
       /motor/forward
       /motor/backward
       /motor/stop
*/

#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>

// =====================================================
// Camera Pin Mapping
// =====================================================

#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM     0

#define SIOD_GPIO_NUM    26
#define SIOC_GPIO_NUM    27

#define Y9_GPIO_NUM      35
#define Y8_GPIO_NUM      34
#define Y7_GPIO_NUM      39
#define Y6_GPIO_NUM      36
#define Y5_GPIO_NUM      21
#define Y4_GPIO_NUM      19
#define Y3_GPIO_NUM      18
#define Y2_GPIO_NUM       5

#define VSYNC_GPIO_NUM   25
#define HREF_GPIO_NUM    23
#define PCLK_GPIO_NUM    22

// =====================================================
// GPIO Assignments
// =====================================================

#define LED1_PIN         2

#define MOTOR_PIN_A      16
#define MOTOR_PIN_B      17

// =====================================================
// Access Point Configuration
// =====================================================

const char* AP_SSID = "ESP32-CAM";
const char* AP_PASS = "12345678";

WebServer server(80);

// =====================================================
// MJPEG Stream Handler
// =====================================================

void handleJPGStream()
{
    WiFiClient client = server.client();

    String response =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";

    server.sendContent(response);

    while (client.connected())
    {
        camera_fb_t * fb = esp_camera_fb_get();

        if (!fb)
        {
            Serial.println("Camera capture failed");
            continue;
        }

        server.sendContent("--frame\r\n");
        server.sendContent("Content-Type: image/jpeg\r\n");
        server.sendContent("Content-Length: " + String(fb->len) + "\r\n\r\n");

        client.write(fb->buf, fb->len);
        server.sendContent("\r\n");

        esp_camera_fb_return(fb);

        if (!client.connected())
        {
            break;
        }

        delay(30);
    }
}

// =====================================================
// Single JPEG Capture
// =====================================================

void handleCapture()
{
    camera_fb_t * fb = esp_camera_fb_get();

    if (!fb)
    {
        server.send(500, "text/plain", "Camera Capture Failed");
        return;
    }

    server.sendHeader("Content-Type", "image/jpeg");
    server.sendHeader("Content-Length", String(fb->len));

    WiFiClient client = server.client();
    client.write(fb->buf, fb->len);

    esp_camera_fb_return(fb);
}

// =====================================================
// LED Endpoints
// =====================================================

void ledOn()
{
    digitalWrite(LED1_PIN, HIGH);
    server.send(200, "text/plain", "LED ON");
}

void ledOff()
{
    digitalWrite(LED1_PIN, LOW);
    server.send(200, "text/plain", "LED OFF");
}

// =====================================================
// Motor Control Endpoints
// =====================================================

void motorForward()
{
    digitalWrite(MOTOR_PIN_A, HIGH);
    digitalWrite(MOTOR_PIN_B, LOW);

    server.send(200, "text/plain", "Motor Forward");
}

void motorBackward()
{
    digitalWrite(MOTOR_PIN_A, LOW);
    digitalWrite(MOTOR_PIN_B, HIGH);

    server.send(200, "text/plain", "Motor Backward");
}

void motorStop()
{
    digitalWrite(MOTOR_PIN_A, LOW);
    digitalWrite(MOTOR_PIN_B, LOW);

    server.send(200, "text/plain", "Motor Stop");
}

// =====================================================
// Root Page
// =====================================================

void handleRoot()
{
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>ESP32 Camera</title>
</head>
<body>
<h2>ESP32 Camera Stream</h2>

<img src="/stream" width="640">

<hr>

<h3>LED Control</h3>
<a href="/led1/on">LED ON</a><br>
<a href="/led1/off">LED OFF</a><br>

<hr>

<h3>Motor Control</h3>
<a href="/motor/forward">Forward</a><br>
<a href="/motor/backward">Backward</a><br>
<a href="/motor/stop">Stop</a><br>

</body>
</html>
)rawliteral";

    server.send(200, "text/html", html);
}

// =====================================================
// Camera Initialization
// =====================================================

bool initCamera()
{
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

    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;

    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;

    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;

    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    if (psramFound())
    {
        config.frame_size = FRAMESIZE_VGA;
        config.jpeg_quality = 10;
        config.fb_count = 2;
    }
    else
    {
        config.frame_size = FRAMESIZE_QVGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
    }

    esp_err_t err = esp_camera_init(&config);

    if (err != ESP_OK)
    {
        Serial.printf("Camera Init Failed: 0x%x\n", err);
        return false;
    }

    sensor_t * s = esp_camera_sensor_get();

    if (s)
    {
        s->set_brightness(s, 0);
        s->set_contrast(s, 0);
        s->set_saturation(s, 0);
    }

    return true;
}

// =====================================================
// Setup
// =====================================================

void setup()
{
    Serial.begin(115200);
    Serial.println();

    pinMode(LED1_PIN, OUTPUT);
    digitalWrite(LED1_PIN, LOW);

    pinMode(MOTOR_PIN_A, OUTPUT);
    pinMode(MOTOR_PIN_B, OUTPUT);

    digitalWrite(MOTOR_PIN_A, LOW);
    digitalWrite(MOTOR_PIN_B, LOW);

    if (!initCamera())
    {
        Serial.println("Camera initialization failed!");
        while (true)
        {
            delay(1000);
        }
    }

    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);

    IPAddress ip = WiFi.softAPIP();

    Serial.println();
    Serial.println("=================================");
    Serial.println("Access Point Started");
    Serial.print("IP Address: ");
    Serial.println(ip);
    Serial.println("Stream URL:");
    Serial.print("http://");
    Serial.print(ip);
    Serial.println("/stream");
    Serial.println("=================================");

    server.on("/", HTTP_GET, handleRoot);

    server.on("/stream", HTTP_GET, handleJPGStream);
    server.on("/capture", HTTP_GET, handleCapture);

    server.on("/led1/on", HTTP_GET, ledOn);
    server.on("/led1/off", HTTP_GET, ledOff);

    server.on("/motor/forward", HTTP_GET, motorForward);
    server.on("/motor/backward", HTTP_GET, motorBackward);
    server.on("/motor/stop", HTTP_GET, motorStop);

    server.begin();

    Serial.println("Web server started");
}

// =====================================================
// Loop
// =====================================================

void loop()
{
    server.handleClient();
}
