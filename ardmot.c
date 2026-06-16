// =====================================
// SIMPLE MOTOR TEST – ALL WHEELS FORWARD
// =====================================

// Motor A (left side)
#define IN1 7
#define IN2 8
#define ENA 11

// Motor B (right side)
#define IN3 9
#define IN4 10
#define ENB 12

void setup() {
  // Set motor control pins as outputs
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);

  // Set both motors to forward, full speed
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, 255);   // left motor max speed

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  analogWrite(ENB, 255);   // right motor max speed
}

void loop() {
  // Motors keep running forever
}
