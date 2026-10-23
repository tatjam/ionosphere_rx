// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <stdint.h>

#define MANCHESTER_BUFFER_BYTES 16

#define FSK_PIN   PA3
#define FSK_PORT  PORTA
#define FSK_DDR   DDRA
#define FSK_PCMSK PCMSK0
#define FSK_PCINT PCINT3
#define FSK_PCIE  PCIE0

#define FSK_TIMER_SCALER (_BV(CS00) | _BV(CS01))

extern volatile uint8_t manchester_idle_events;

void manchester_init();

// Returns the number of calls to manchester_read_next() that can be safely
// done at this moment
uint8_t manchester_get_next_count();

// Only call manchester_read_next() if manchester_get_next_count() > 0,
// returns the byte from the buffer
uint8_t manchester_read_next();

// Returns the overflow counter, if it's > 0, data has been lost, and
// the reader must call this function, and afterwards he can inspect the
// buffer (either to clear it or to extract useful info)
uint8_t manchester_get_overflow();

void manchester_clear_overflow();
