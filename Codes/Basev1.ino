/*
 * Arduino Uno – Step counter / pour sequencer
 * ------------------------------------------------
 * SoftwareSerial : RX = D2, TX = D3   (9600 baud)
 *
 * Received commands:
 *    "step"    -> step counter +1
 *    "ready"   -> send "run" 10 ms later
 *    "poured"  -> send "run" 30 ms later
 *
 * Sent commands:
 *    "run"
 *    "pour"
 *
 * Hall sensor on A0. When the sensor reading goes above HALL_THRESHOLD
 * the step counter is zeroed and the pour sequence is re-armed.
 * After the first zeroing, a "pour" is sent when the step count reaches
 * each of the four configured positions (0, 1/4, 1/2, 3/4 of 2048).
 * No "pour" is ever sent before the first zeroing.
 */

#include <SoftwareSerial.h>

/* ---------------- user configuration ---------------- */
const uint8_t  RX_PIN = 2;
const uint8_t  TX_PIN = 3;

const uint8_t  HALL_PIN       = A0;
const int      HALL_THRESHOLD = 500;     // adjust to your Hall sensor

const unsigned int MAX_STEPS     = 100000; // counter saturation
const unsigned int STEPS_PER_ROT = 14057;  // one full rotation 2048 * 151/22

/* Four "pour" trigger positions, in steps after zeroing.
 * They are independent constants - change them individually if needed. */
const unsigned int POUR_POS_0 = 0;       // zero position
const unsigned int POUR_POS_1 = 3514;     // 1/4 rotation
const unsigned int POUR_POS_2 = 7028;    // 1/2 rotation
const unsigned int POUR_POS_3 = 10542;    // 3/4 rotation

const unsigned int POUR_POS[4] = { POUR_POS_0, POUR_POS_1,
                                   POUR_POS_2, POUR_POS_3 };
const uint8_t      POUR_COUNT  = 4;

/* ---------------- state ---------------- */
SoftwareSerial mySerial(RX_PIN, TX_PIN);

unsigned int steps         = 0;
bool         firstZeroDone = false;  // "pour" blocked until true
uint8_t      nextPour      = 0;      // index into POUR_POS[]

bool          runPending = false;
unsigned long runAt      = 0;

bool hallAbove = false;              // for rising-edge detection

char    rxBuf[16];
uint8_t rxLen = 0;

/* ---------------- helpers ---------------- */
void sendCommand(const char *cmd) {
  mySerial.write(cmd);
  mySerial.write('\n');
}

void processCommand(const char *cmd) {
  if (strcmp(cmd, "step") == 0) {
    if (steps < MAX_STEPS) steps++;

  } else if (strcmp(cmd, "ready") == 0) {
    // send "run" 10 ms after "ready"
    runAt      = millis() + 10;
    runPending = true;

  } else if (strcmp(cmd, "poured") == 0) {
    // send "run" 30 ms after "poured"
    runAt      = millis() + 30;
    runPending = true;
  }
}

void readSoftwareSerial() {
  while (mySerial.available()) {
    char c = mySerial.read();

    if (c == '\n' || c == '\r') {
      if (rxLen > 0) {                 // line-terminated command
        rxBuf[rxLen] = '\0';
        processCommand(rxBuf);
        rxLen = 0;
      }
    } else {
      if (rxLen < sizeof(rxBuf) - 1) {
        rxBuf[rxLen++] = c;
        rxBuf[rxLen]   = '\0';

        // also accept commands that arrive without any terminator
        if (strcmp(rxBuf, "step")   == 0 ||
            strcmp(rxBuf, "ready")  == 0 ||
            strcmp(rxBuf, "poured") == 0) {
          processCommand(rxBuf);
          rxLen = 0;
        }
      } else {
        rxLen = 0;                     // overflow – drop garbage
      }
    }
  }
}

/* ---------------- setup / loop ---------------- */
void setup() {
  pinMode(HALL_PIN, INPUT);
  pinMode(A1, INPUT_PULLUP);
  Serial.begin(9600);
  while(!Serial){
    continue;
  };
  mySerial.begin(9600);

  // Optional debug port:
  // Serial.begin(9600);

  hallAbove = analogRead(HALL_PIN) > HALL_THRESHOLD;
}

void loop() {
  readSoftwareSerial();

  /* ----- Hall sensor: zero the counter on rising edge ----- */
  bool above = analogRead(HALL_PIN) > HALL_THRESHOLD;
  if (above && !hallAbove && steps >= 100) {           // new pulse
    steps         = 0;
    firstZeroDone = true;              // "pour" now allowed
    nextPour      = 0;                 // re-arm the pour sequence
  }
  hallAbove = above;
  /*if(steps%100 == 0){
    Serial.println(steps);
  };*/
  /* ----- Send "pour" at the configured positions ----- */
  Serial.println(analogRead(A1));
  if (firstZeroDone) {
    while (nextPour < POUR_COUNT && steps >= POUR_POS[nextPour]) {
      if(nextPour == 0 && analogRead(A1) > 256 && analogRead(A1) < 650){sendCommand("pour"); Serial.println("pour"); delay(30);};
      nextPour++;
    }
  }

  /* ----- Delayed "run" reply (10 ms after ready / 30 ms after poured) ----- */
  if (runPending && (long)(millis() - runAt) >= 0) {
    runPending = false;
    sendCommand("run");
  }
}