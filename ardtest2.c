// ==============================================
// PIPE INSPECTION BOT – OBSTACLE‑RESPONSIVE TEST
// ==============================================
// - Both motors run clockwise (forward) continuously
// - Ultrasonic sensor measures distance
// - If distance < 30 cm → slow down to 50% speed
// - If distance < 15 cm → full stop
// - If distance ≥ 30 cm → full speed forward
// - All sensor data printed to Serial Monitor
// ==============================================

#include <Servo.h>
#include <DHT.h>

// ---------- Pin definitions (must match your wiring) ----------
#define DHTPIN 2
#define DHTTYPE DHT11   // change to DHT22 if needed
DHT dht(DHTPIN, DHTTYPE);

#define TRIG 3
#define ECHO 4
#define IR_PIN 5

Servo camServo;
#define SERVO_PIN 6

#define LED_FRONT A0
#define LED_REAR  A1

// Motor driver (L298N)
#define IN1 7
#define IN2 8
#define IN3 9
#define IN4 10
#define ENA 11   // left speed
#define ENB 12   // right speed

// Speed settings (0–255)
const int FULL_SPEED = 200;   // adjust as needed
const int SLOW_SPEED = 100;   // 50% of full speed

// Distance thresholds (in cm)
const float STOP_THRESHOLD   = 15.0;
const float SLOW_THRESHOLD   = 30.0;

// Global variable for obstacle detection status
bool obstacleDetected = false;

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- Obstacle-Responsive Motor Test ---");

  // DHT
  dht.begin();

  // Ultrasonic
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  // IR sensor
  pinMode(IR_PIN, INPUT);

  // LEDs
  pinMode(LED_FRONT, OUTPUT);
  pinMode(LED_REAR, OUTPUT);
  digitalWrite(LED_FRONT, LOW);
  digitalWrite(LED_REAR, LOW);

  // Servo (center)
  camServo.attach(SERVO_PIN);
  camServo.write(90);

  // Motor pins
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);

  stopMotors();
  delay(1000);
  Serial.println("Robot ready – moving forward with obstacle detection.");
}

void loop() {
  // ========== 1. Read all sensors ==========
  // Temperature & humidity
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  // Ultrasonic distance
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  long duration = pulseIn(ECHO, HIGH, 30000); // 30ms timeout
  float distance = (duration == 0) ? 400.0 : duration * 0.0343 / 2.0; // if timeout, treat as far away

  // IR sensor (LOW = obstacle)
  int ir = digitalRead(IR_PIN);

  // ========== 2. Print sensor data ==========
  Serial.print("Temp: ");
  if (isnan(t)) Serial.print("Err"); else Serial.print(t, 1);
  Serial.print("°C  Hum: ");
  if (isnan(h)) Serial.print("Err"); else Serial.print(h, 1);
  Serial.print("%  Dist: ");
  Serial.print(distance, 1);
  Serial.print(" cm  IR: ");
  Serial.println(ir == LOW ? "OBSTACLE" : "CLEAR");

  // ========== 3. LED indication ==========
  // Front LED on when obstacle within SLOW_THRESHOLD
  if (distance <= SLOW_THRESHOLD) {
    digitalWrite(LED_FRONT, HIGH);
    digitalWrite(LED_REAR, HIGH);
  } else {
    digitalWrite(LED_FRONT, LOW);
    digitalWrite(LED_REAR, LOW);
  }

  // ========== 4. Motor speed control ==========
  if (distance <= STOP_THRESHOLD) {
    // Very close – stop
    setMotorA(0);
    setMotorB(0);
    Serial.println("-> STOPPED (distance < 15 cm)");
  } else if (distance <= SLOW_THRESHOLD) {
    // Moderate range – slow down
    setMotorA(SLOW_SPEED);
    setMotorB(SLOW_SPEED);
    Serial.println("-> SLOWED (distance < 30 cm)");
  } else {
    // Clear path – full speed forward
    setMotorA(FULL_SPEED);
    setMotorB(FULL_SPEED);
    Serial.println("-> FULL SPEED");
  }

  // ========== 5. Servo sweep (optional, for visual check) ==========
  // Slowly pan camera back and forth while moving
  static unsigned long lastSweep = 0;
  static int servoAngle = 90;
  static int sweepDir = 1;
  if (millis() - lastSweep > 50) {
    lastSweep = millis();
    servoAngle += sweepDir * 2;
    if (servoAngle >= 150 || servoAngle <= 30) sweepDir *= -1;
    camServo.write(servoAngle);
  }

  // Small delay to avoid flooding Serial
  delay(200);
}

// ---------- Motor helpers ----------
void setMotorA(int speed) {  // left side
  if (speed > 0) {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    analogWrite(ENA, speed);
  } else if (speed < 0) {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
    analogWrite(ENA, -speed);
  } else {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    analogWrite(ENA, 0);
  }
}

void setMotorB(int speed) {  // right side
  if (speed > 0) {
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
    analogWrite(ENB, speed);
  } else if (speed < 0) {
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
    analogWrite(ENB, -speed);
  } else {
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);
    analogWrite(ENB, 0);
  }
}

void stopMotors() {
  setMotorA(0);
  setMotorB(0);
}
