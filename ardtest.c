// ==============================================
// PIPE INSPECTION BOT - SENSOR & MOTOR TEST
// ==============================================
// Use this sketch to verify:
//  - DHT11 temperature/humidity
//  - Ultrasonic distance
//  - IR obstacle sensor
//  - LED front/rear
//  - Both motors run clockwise (forward) for 3 sec
//  - Servo sweeps
// ==============================================

#include <Servo.h>
#include <DHT.h>

// ---------- Pin Definitions (must match your wiring) ----------
#define DHTPIN 2
#define DHTTYPE DHT11   // change to DHT22 if you have that
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

// ============================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n--- PipeBot Sensor & Motor Test ---");

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

  // Servo
  camServo.attach(SERVO_PIN);
  camServo.write(90);  // center

  // Motor pins
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);

  stopMotors();

  Serial.println("Setup complete.");
  delay(1000);
}

void loop() {
  // -------------------------------------------------------
  // 1. TEST SENSORS (print once per loop, loop runs slow)
  // -------------------------------------------------------
  // DHT11
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  Serial.print("Temp: "); 
  if (isnan(t)) Serial.print("Error"); else Serial.print(t, 1);
  Serial.print(" °C  |  Humidity: ");
  if (isnan(h)) Serial.print("Error"); else Serial.print(h, 1);
  Serial.println(" %");

  // Ultrasonic
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  long duration = pulseIn(ECHO, HIGH, 30000); // timeout 30ms
  float distance = (duration == 0) ? -1 : duration * 0.0343 / 2.0;
  Serial.print("Distance: ");
  Serial.print(distance, 1);
  Serial.println(" cm");

  // IR sensor (LOW = obstacle, HIGH = clear)
  int ir = digitalRead(IR_PIN);
  Serial.print("IR: ");
  Serial.println(ir == LOW ? "OBSTACLE" : "CLEAR");

  // -------------------------------------------------------
  // 2. BLINK LEDs briefly
  // -------------------------------------------------------
  Serial.println("Blinking LEDs...");
  digitalWrite(LED_FRONT, HIGH);
  digitalWrite(LED_REAR, HIGH);
  delay(500);
  digitalWrite(LED_FRONT, LOW);
  digitalWrite(LED_REAR, LOW);

  // -------------------------------------------------------
  // 3. SERVO SWEEP (test camera pan)
  // -------------------------------------------------------
  Serial.println("Servo sweep...");
  for (int angle = 0; angle <= 180; angle += 30) {
    camServo.write(angle);
    delay(200);
  }
  camServo.write(90);  // back to center
  delay(500);

  // -------------------------------------------------------
  // 4. MOTOR TEST – CLOCKWISE (forward) for 3 seconds
  // -------------------------------------------------------
  // Note: "clockwise" depends on motor wiring.
  // This sets both sides to forward at ~60% speed.
  Serial.println("Motors: CLOCKWISE (forward) for 3 sec");
  setMotorA(150);  // left side (IN1,IN2) forward, speed ~150/255
  setMotorB(150);  // right side forward
  delay(3000);
  stopMotors();
  Serial.println("Motors stopped.");

  // -------------------------------------------------------
  // 5. Wait before repeating the test
  // -------------------------------------------------------
  Serial.println("Test cycle complete. Waiting 5 sec...\n");
  delay(5000);
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
