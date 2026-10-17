// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later
#include "link.h"
#include <avr/io.h>

static void set_fsk(uint16_t v) { ICR1 = v; }

static void toggle_off_fsk()
{
	// Set clock source = none
	TCCR1B &= ~(_BV(CS12) | _BV(CS11) | _BV(CS10));
}

static void toggle_on_fsk()
{
	// Restart counter
	TCNT1 = 0;
	// Set clock source = I/O clock
	TCCR1B |= _BV(CS10);
}

void link_init()
{
	// Set the pin to output and the timer to output on it
	DDRB |= _BV(PB1);

	// We setup the "Fast PWM" mode, with TOP = IRC1, WGM = 14
	TCCR1A |= _BV(WGM11);
	TCCR1B |= _BV(WGM13) | _BV(WGM12);

	// Toggle output (PB1) on BOTTOM
	TCCR1A |= _BV(COM1A0);

	// It doesn't matter where we place OCR1A, it just phase shifts
	// the signal, as the counter always goes 0 = BOTTOM -> TOP
	OCR1A = 0;

	toggle_off_fsk();
}

bool link_task() { return false; }
