// This was made by Claude Code and I reviewed it, but treat with care!
// OUT_PIN choices (ATmega328P):
//   9  -> Timer1 OC1A
//   10 -> Timer1 OC1B
//   3  -> Timer2 OC2B
//
// Encoding:
//   bit 1 = low->high (space -> mark), bit 0 = high->low (mark -> space)
//   LSB first, idle = mark (high).
//
// Frame format:
//   idle | SYNC 0x55 | word | word | ... (1..16 words) | gap | idle
//   A word is 2 bytes. A frame ends when the queue has no full word left,
//   or when FRAME_MAX_PAYLOAD bytes have been sent (forced resync gap).

#include <Arduino.h>

// ---------------- Configuration ----------------
#define OUT_PIN 9              // 9, 10 or 3
#define HOST_BAUD 115200       // serial link from the PC
#define DEFAULT_MARK 150000UL  // Hz, "high" tone (and idle)
#define DEFAULT_SPACE 300000UL // Hz, "low" tone
#define DEFAULT_BAUD 1200UL    // Manchester bit rate (bits/s)
#define FREQ_MIN 150000UL
#define FREQ_MAX 400000UL
#define ESC 0x1B             // command escape, see below
#define SYNC 0x55            // frame start byte
#define GAP_BITS 4           // idle bit-times between frames
#define FRAME_MAX_PAYLOAD 32 // max payload bytes per frame (16 words)
// -----------------------------------------------

#if OUT_PIN == 9 || OUT_PIN == 10
#define USE_TIMER1
#elif OUT_PIN == 3
#define USE_TIMER2
#else
#error "OUT_PIN must be 9, 10 or 3"
#endif

static uint32_t markHz = DEFAULT_MARK;
static uint32_t spaceHz = DEFAULT_SPACE;
static uint32_t bitUs = 1000000UL / DEFAULT_BAUD;
static uint16_t toggleHz = 0;
static bool toggleState = false;
static uint32_t toggleNextUs = 0;
static bool active = false;
static bool inGap = false;
static uint8_t curByte, slot, sent; // sent = payload bytes in current frame
static uint32_t nextUs, gapEndUs;

static uint16_t markTop, spaceTop, curTop = 0;

// ---------- Timer setup ----------
static uint16_t hzToTop(uint32_t hz) {
  hz = constrain(hz, FREQ_MIN, FREQ_MAX);
  return (uint16_t)((F_CPU + hz / 2) / hz) - 1; // period = TOP+1 timer ticks
}

static uint32_t topToHz(uint16_t top) { return F_CPU / ((uint32_t)top + 1); }

static void setTop(uint16_t top) {
  if (top == curTop)
    return; // same tone: don't disturb the carrier
  curTop = top;
  uint16_t duty = ((top + 1) >> 1) - 1; // ~50% duty
#ifdef USE_TIMER1
  uint8_t s = SREG;
  cli();
  TCNT1 = 0; // avoid counting past the new TOP (long wrap)
  ICR1 = top;
#if OUT_PIN == 9
  OCR1A = duty;
#else
  OCR1B = duty;
#endif
  SREG = s;
#else
  OCR2A = top;
  OCR2B = duty;
#endif
}

static void timerInit() {
  pinMode(OUT_PIN, OUTPUT);
#ifdef USE_TIMER1
  // Mode 14: fast PWM, TOP = ICR1, prescaler 1
  TCCR1A = _BV(WGM11) | (OUT_PIN == 9 ? _BV(COM1A1) : _BV(COM1B1));
  TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS10);
#else
  // Mode 7: fast PWM, TOP = OCR2A, prescaler 1, output on OC2B
  TCCR2A = _BV(COM2B1) | _BV(WGM21) | _BV(WGM20);
  TCCR2B = _BV(WGM22) | _BV(CS20);
#endif
}

static void updateTones() {
  markTop = hzToTop(markHz);
  spaceTop = hzToTop(spaceHz);
}

static inline void setBit(bool b) { setTop(b ? markTop : spaceTop); }

// ---------- TX ring buffer ----------
#define RB_SIZE 128
static uint8_t rb[RB_SIZE];
static uint8_t rbHead = 0, rbTail = 0, rbCount = 0;

static inline bool rbPush(uint8_t b) {
  if (rbCount >= RB_SIZE)
    return false;
  rb[rbHead] = b;
  rbHead = (rbHead + 1) % RB_SIZE;
  rbCount++;
  return true;
}
static inline uint8_t rbPop() {
  uint8_t b = rb[rbTail];
  rbTail = (rbTail + 1) % RB_SIZE;
  rbCount--;
  return b;
}

// ---------- Serial input + commands ----------
// Data bytes are sent as-is. To send a literal 0x1B, send 0x1B 0x1B.
// A command is ESC, a letter, a decimal number and a newline:
//   ESC M200000\n   mark frequency in Hz
//   ESC S250000\n   space frequency in Hz
//   ESC B1200\n     Manchester bit rate in bits/s
static uint8_t inState = 0; // 0 data, 1 after ESC, 2 collecting command
static char cmd[12];
static uint8_t cmdLen = 0;

static void runCommand() {
  cmd[cmdLen] = 0;
  long v = atol(cmd + 1);
  switch (cmd[0]) {
  case 'M':
  case 'm':
    markHz = constrain(v, (long)FREQ_MIN, (long)FREQ_MAX);
    break;
  case 'S':
  case 's':
    spaceHz = constrain(v, (long)FREQ_MIN, (long)FREQ_MAX);
    break;
  case 'B':
  case 'b':
    if (v >= 10 && v <= 20000)
      bitUs = 1000000UL / v;
    break;
  case 'T':
  case 't':
    if (v >= 0 && v <= 1000) {
      toggleHz = v;
      // reset the data engine and start from a clean state
      active = false;
      inGap = false;
      rbHead = rbTail = rbCount = 0;
      toggleNextUs = micros();
      toggleState = true;
      if (toggleHz == 0)
        setBit(1); // back to idle mark
    }
    break;
  default:
    Serial.println(F("ERR"));
    return;
  }
  updateTones();
  curTop = 0; // force re-apply on the next half-bit
  Serial.print(F("OK mark="));
  Serial.print(topToHz(markTop));
  Serial.print(F(" space="));
  Serial.print(topToHz(spaceTop));
  Serial.print(F(" bitUs="));
  Serial.println(bitUs);
  Serial.print(F(" toggleHz="));
  Serial.println(toggleHz);
}

static void pollSerial() {
  while (Serial.available() && rbCount < RB_SIZE - 1) {
    uint8_t b = Serial.read();
    switch (inState) {
    case 0:
      if (b == ESC)
        inState = 1;
      else if (toggleHz == 0)
        rbPush(b);
      break;
    case 1:
      if (b == ESC) {
        rbPush(ESC);
        inState = 0;
      } else {
        cmd[0] = b;
        cmdLen = 1;
        inState = 2;
      }
      break;
    case 2:
      if (b == '\n' || b == '\r') {
        runCommand();
        inState = 0;
      } else if (cmdLen < sizeof(cmd) - 1)
        cmd[cmdLen++] = b;
      break;
    }
  }
}

// ---------- Manchester FSK bit engine ----------
// Each data bit is two half-bits: first half = !bit, second half = bit.
// A frame starts with SYNC, so the receiver sees a boundary edge followed by
// a mid-bit edge exactly half a bit later.

static void txTask() {
  const uint32_t half = bitUs >> 1;

  if (!active) {
    if (inGap && (int32_t)(micros() - gapEndUs) >= 0)
      inGap = false;
    if (rbCount >= 2 && !inGap) { // at least one whole word queued
      curByte = SYNC;
      slot = 0;
      sent = 0;
      active = true;
      setBit(!(curByte & 1)); // first half = opposite of bit
      nextUs = micros() + half;
    }
    return;
  }

  if ((int32_t)(micros() - nextUs) < 0)
    return;
  nextUs += half; // fixed schedule, no drift
  slot++;

  if (slot >= 16) {
    // Decide only at word boundaries (sent even). The second byte of a word
    // is always queued, because rbCount >= 2 was checked at the word start.
    bool wordStart = (sent & 1) == 0;
    bool cont = wordStart ? (sent < FRAME_MAX_PAYLOAD && rbCount >= 2) : true;
    if (cont) {
      curByte = rbPop();
      sent++;
      slot = 0;
    } else {
      active = false; // end of frame
      setBit(1);      // idle on mark
      inGap = true;
      gapEndUs = micros() + GAP_BITS * bitUs;
      return;
    }
  }
  bool b = (curByte >> (slot >> 1)) & 1;
  setBit((slot & 1) ? b : !b); // first half: !b, second: b
}

static void toggleTask() {
  const uint32_t halfUs = 500000UL / toggleHz; // half period per tone
  if ((int32_t)(micros() - toggleNextUs) >= 0) {
    toggleNextUs += halfUs; // fixed schedule, no drift
    toggleState = !toggleState;
    setBit(toggleState);
  }
}

void setup() {
  Serial.begin(HOST_BAUD);
  updateTones();
  timerInit();
  setBit(1); // idle = mark tone
  toggleNextUs = micros();
}

void loop() {
  pollSerial();
  if (toggleHz != 0)
    toggleTask();
  txTask();
}
