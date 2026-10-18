// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <stdbool.h>
#include <stdint.h>

// How many Timer1 ticks per second (ideally)
#define TICKS_PER_S 62500u
// How many more / less ticks of Timer1 are allowed to adjust the timing,
// ensure TICKS_PER_S + MAX_SLEW <= 65534
#define MAX_SLEW 2000u

// 50 ticks is roughly 800us
#define SLOT_RATE_MIN 50

_Static_assert((uint32_t) TICKS_PER_S + (uint32_t) MAX_SLEW < 65535, "MAX_SLEW too large");

// - Uses Timer1 with 256 prescaler to get a tick each 16us
// - Outputs a 2-second square wave on PB1 (pin 9 in Arduino Nano), i.e.
// 	 the signal switches each second as measured by us
// - TODO: Can receive an external PPS on PB0 (pin 8 in Arduin Nano)
void timebase_init();

// The function 'timebase_on_slot' is triggered starting at a given second, at
// the given rate in ticks (16us) since that second. The first trigger is at the
// start of the given second.
//
// If start_sec has already passed, the function will return false, and the
// slots will never start ticking.
//
// If slots are already running, nothing special happens, the rate will change on the given
// next second.
//
// Rate must be SLOT_RATE_MIN <= rate < TICKS_PER_S
bool timebase_start_slot(uint32_t start_sec, uint16_t rate);

// Stops the slot interrupt after next tick (but next tick will be called!)
void timebase_stop_slot();

// Called every time the timebase slot triggers from the ISR, so be quick,
// weakly defined to do nothing
void timebase_on_slot(uint32_t tick_num);

uint32_t timebase_get_second();
uint16_t timebase_get_tick();
