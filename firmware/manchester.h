// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <stdint.h>

#define MANCHESTER_BUFFER_BYTES 16
extern uint8_t manchester_buffer[MANCHESTER_BUFFER_BYTES];

void manchester_init();

// Returns the number of calls to manchester_read_next() that can be safely
// done at this moment
uint8_t manchester_get_next_count();

// Only call manchester_read_next() if manchester_get_next_count() > 0,
// returns the byte from the buffer
uint8_t manchester_read_next();

// Returns the overflow counter, if it's > 1, data has been lost
uint8_t manchester_get_overflow();
