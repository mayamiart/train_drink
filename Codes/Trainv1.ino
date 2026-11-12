#include <SoftwareSerial.h>

// ===== CONSTANTS =====
#define POURING_TIME 1000   // milliseconds — change as needed

// ===== PIN MAP =====
const uint8_t PIN_A = 8;      // IN1  -> ULN2003
const uint8_t PIN_B = 9;     // IN2
const uint8_t PIN_C = 10;     // IN3
const uint8_t PIN_D = 11;     // IN4
const uint8_t POUR_PIN = 13;

// ===== SPEED (10 RPM, half-step) =====
// 28BYJ-48: 64 half-steps per inner revolution, ~63.68:1 gearbox
//   -> ~2038 half-steps per OUTPUT revolution
//   10 RPM = 2038 * 10 / 60 = 339.7 half-steps/sec
//   -> 1000000 / 339.7 = ~2944 us per half-step
const unsigned int HALF_STEP_US = 3000;

SoftwareSerial serialBT(2, 3);   // RX = D2, TX = D3

// Half-step sequence (8 states), one/two coils energized per step
const uint8_t HALF_STEP_TABLE[8][4] = {
  {1, 0, 0, 0},
  {1, 1, 0, 0},
  {0, 1, 0, 0},
  {0, 1, 1, 0},
  {0, 0, 1, 0},
  {0, 0, 1, 1},
  {0, 0, 0, 1},
  {1, 0, 0, 1}
};

bool running = false;
uint8_t stepIndex = 0;

// ---------------------------------------------------
inline void writeStep(uint8_t idx) {
  serialBT.println("step");
  digitalWrite(PIN_A, HALF_STEP_TABLE[idx][0]);
  digitalWrite(PIN_B, HALF_STEP_TABLE[idx][1]);
  digitalWrite(PIN_C, HALF_STEP_TABLE[idx][2]);
  digitalWrite(PIN_D, HALF_STEP_TABLE[idx][3]);
}

// ---------------------------------------------------
void setup() {
  pinMode(PIN_A, OUTPUT);
  pinMode(PIN_B, OUTPUT);
  pinMode(PIN_C, OUTPUT);
  pinMode(PIN_D, OUTPUT);
  pinMode(POUR_PIN, OUTPUT);

  digitalWrite(PIN_A, LOW);
  digitalWrite(PIN_B, LOW);
  digitalWrite(PIN_C, LOW);
  digitalWrite(PIN_D, LOW);
  digitalWrite(POUR_PIN, LOW);

  serialBT.begin(9600);
  delay(50);
  serialBT.println("ready");
}

// ---------------------------------------------------
void loop() {
  // ---- Commands ----
  if (serialBT.available()) {
    String cmd = serialBT.readStringUntil('\n');
    cmd.trim();

    if (cmd == "run") {
      running = true;
    }
    else if (cmd == "stop") {
      running = false;
      // De-energize coils so the motor is not held (saves heat / current)
      digitalWrite(PIN_A, LOW);
      digitalWrite(PIN_B, LOW);
      digitalWrite(PIN_C, LOW);
      digitalWrite(PIN_D, LOW);
    }
    else if (cmd == "pour") {
      digitalWrite(POUR_PIN, HIGH);
      delay(POURING_TIME);
      digitalWrite(POUR_PIN, LOW);
      serialBT.println("poured");
    }
  }

  // ---- Half-step the motor one full electrical cycle (8 half-steps) ----
  if (running) {
    for (uint8_t i = 0; i < 8; i++) {
      writeStep(stepIndex);
      stepIndex = (stepIndex + 1) & 0x07;      // 0..7 wrap
      delayMicroseconds(HALF_STEP_US);
    }
  }
}