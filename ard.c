#include <Servo.h>
#include <DHT.h>

// --- Pin definitions ---
#define DHTPIN 2
#define DHTTYPE DHT11   // or DHT22
DHT dht(DHTPIN, DHTTYPE);

#define TRIG 3
#define ECHO 4
#define IR_PIN 5

Servo camServo;
#define SERVO_PIN 6

#define LED_FRONT A0
#define LED_REAR  A1

// Motor driver
#define IN1 7
#define IN2 8
#define IN3 9
#define IN4 10
#define ENA 11
#define ENB 12

// --- Global variables ---
unsigned long lastSensorSend = 0;

void setup() {
  Serial.begin(115200);  // Serial to ESP32
  dht.begin();
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(IR_PIN, INPUT);
  pinMode(LED_FRONT, OUTPUT);
  pinMode(LED_REAR, OUTPUT);
  digitalWrite(LED_FRONT, LOW);
  digitalWrite(LED_REAR, LOW);

  camServo.attach(SERVO_PIN);
  camServo.write(90);

  // Motor pins
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT); pinMode(ENB, OUTPUT);

  // Stop motors
  stopMotors();
  Serial.println("Arduino ready");
}

void loop() {
  // --- 1. Process incoming commands from ESP32 ---
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.startsWith("M ")) {           // MOTORS: "M left right"
      int sp1 = cmd.indexOf(' ', 2);
      int leftSpeed = cmd.substring(2, sp1).toInt();
      int rightSpeed = cmd.substring(sp1+1).toInt();
      setMotorA(leftSpeed);   // left side
      setMotorB(rightSpeed);  // right side
    }
    else if (cmd.startsWith("S ")) {      // SERVO: "S angle"
      int angle = cmd.substring(2).toInt();
      camServo.write(constrain(angle, 0, 180));
    }
    else if (cmd.startsWith("L1 ")) {     // LED1 on/off
      digitalWrite(LED_FRONT, cmd.substring(3).toInt() ? HIGH : LOW);
    }
    else if (cmd.startsWith("L2 ")) {     // LED2 on/off
      digitalWrite(LED_REAR, cmd.substring(3).toInt() ? HIGH : LOW);
    }
    else if (cmd == "?") {                // Request sensor data
      sendSensorData();
    }
  }

  // --- 2. Automatically send sensor data every 500 ms ---
  if (millis() - lastSensorSend > 500) {
    sendSensorData();
    lastSensorSend = millis();
  }
}

void sendSensorData() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (isnan(t) || isnan(h)) { t = 0; h = 0; }

  // Ultrasonic
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  long duration = pulseIn(ECHO, HIGH, 30000);
  float dist = (duration == 0) ? -1 : duration * 0.0343 / 2.0;

  // IR (LOW = obstacle, HIGH = clear)
  int ir = digitalRead(IR_PIN);

  // Send: "T:23.5 H:45.2 D:14.3 I:1"
  Serial.print("T:"); Serial.print(t, 1);
  Serial.print(" H:"); Serial.print(h, 1);
  Serial.print(" D:"); Serial.print(dist, 1);
  Serial.print(" I:"); Serial.println(ir);
}

void setMotorA(int speed) {  // left side
  if (speed > 0) {
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
    analogWrite(ENA, speed);
  } else if (speed < 0) {
    digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
    analogWrite(ENA, -speed);
  } else {
    digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
    analogWrite(ENA, 0);
  }
}

void setMotorB(int speed) {  // right side
  if (speed > 0) {
    digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
    analogWrite(ENB, speed);
  } else if (speed < 0) {
    digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
    analogWrite(ENB, -speed);
  } else {
    digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
    analogWrite(ENB, 0);
  }
}

void stopMotors() {
  setMotorA(0); setMotorB(0);
}
