// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later
#include "timebase.h"
#include "../shared/critical_section.h"
#include <stdint.h>

static volatile uint32_t second;
static volatile uint32_t slot_tick;
static volatile uint32_t slot_next_start;
static volatile uint16_t slot_next_rate;
static volatile uint16_t slot_rate;

static volatile bool slot_run;

#ifndef ENV_TEST
#include <avr/interrupt.h>
#include <avr/io.h>

uint16_t timebase_hw_read_tick()
{
	return TCNT1;
}

void timebase_hw_init()
{
	// Set the pin to output
	DDRB |= _BV(PB1);

	// Set COMPA to a reasonable default
	OCR1A = TICKS_PER_S - 1;
	OCR1B = 0;

	// Set WGM = 4, CTC with TOP = OCR1A, update of OCR1A is immediate,
	// toggling the output pin on compare match
	TCCR1B = _BV(WGM12);
	TCCR1A = _BV(COM1A0);

	// Set prescaler to divide by 256 (16MHz / 256 = 16us ticks)
	TCCR1B |= _BV(CS12);

	// Enable per-second interrupt
	TIMSK1 |= _BV(OCIE1A);
}

// Triggered when ticks = OCR1A (i.e. every second)
ISR(TIMER1_COMPA_vect)
{
	second++;
	if (slot_run && second == slot_next_start)
	{
		// Send first tick, and set clock for next
		timebase_on_slot(0);
		slot_tick = 1;

		slot_rate = slot_next_rate;
		OCR1B     = slot_rate;

		// Clear any previous trigger of the COMPB interrupt, and enable it
		TIFR1 = _BV(OCF1B);
		TIMSK1 |= _BV(OCIE1B);
	}
}

// Triggered when ticks = OCR1B.
ISR(TIMER1_COMPB_vect)
{
	timebase_on_slot(slot_tick);
	slot_tick++;

	if (slot_run)
	{
		// We have to go through 32 bits sadly if the slot rate is slow
		uint32_t p = (uint32_t) OCR1B + (uint32_t) slot_rate;
		if (p >= TICKS_PER_S)
		{
			p -= TICKS_PER_S;
		}
		OCR1B = p;
	}
	else
	{
		// Disable the interrupt
		TIMSK1 &= ~_BV(OCIE1B);
	}
}

void __attribute__((weak)) timebase_on_slot(uint32_t _) { return; }

#endif

void timebase_init()
{
	second   = 0;
	slot_run = false;
	timebase_hw_init();
}

void timebase_stop_slot()
{
	CRITICAL_SECTION { slot_run = false; }
}

bool timebase_start_slot(uint32_t start_sec, uint16_t rate)
{
	if (rate < SLOT_RATE_MIN || rate >= TICKS_PER_S)
	{
		return false;
	}

	CRITICAL_SECTION
	{
		if (second >= start_sec)
		{
			return false;
		}

		slot_next_start = start_sec;
		slot_next_rate  = rate;
		slot_run        = true;
	}

	return true;
}

uint32_t timebase_get_second()
{
	uint32_t second_read;
	CRITICAL_SECTION { second_read = second; }
	return second_read;
}

uint16_t timebase_get_tick()
{
	uint16_t tick_read;
	CRITICAL_SECTION { tick_read = timebase_hw_read_tick(); }
	return tick_read;
}
