// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later

#include "manchester.h"
#include "../shared/ring_buffer.h"
#include <stdbool.h>

// Forward decl for hardware so file is neat
static inline void manchester_handle_timer_int();
static inline void manchester_handle_pin_change_int();

#ifndef ENV_TEST
#include <avr/io.h>
#include <avr/interrupt.h>

#define FSK

static volatile bool up_counter = false;

static void manchester_hw_init()
{
	// Set PA3 as PCINT0 source
	PCMSK0 |= _BV(PCINT3);
	GIMSK |= _BV(PCIE0);
}

static void manchester_hw_start_counter(bool up, uint8_t initial)
{
	// Stop counter if running
	TCCR0B = 0;

	// Restart counter
	TCNT0 = initial;
	// Clear interrupt if it's already triggered
	TIFR0 |= _BV(TOV0);
	// Enable the overflow interrupt
	TIMSK0 |= _BV(TOIE0);

	up_counter = up;

	// Launch timer
	TCCR0B |= _BV(CS00) | _BV(CS01);
}

static void manchester_hw_start_up_counter() { manchester_hw_start_counter(true, 0); }

static void manchester_hw_start_down_counter(uint8_t start)
{
	// The interface expects it to count down FROM start, so
	// we set everything symmetric (as the HW counter can only
	// count upwards!)
	manchester_hw_start_counter(false, 0xFF - start);
}

static uint8_t manchester_hw_get_counter()
{
	if (up_counter)
	{
		return TCNT0;
	}
	else
	{
		return 0xFF - TCNT0;
	}
}

static uint8_t manchester_hw_get_pin_value() { return PINA & _BV(PA3); }

ISR(TIM0_OVF_vect) { manchester_handle_timer_int(); }

ISR(PCINT0_vect) { manchester_handle_pin_change_int(); }

#define DEBUG(...)
#else
#define DEBUG(...) fprintf(stderr, __VA_ARGS__)
#endif

// This is a ring-buffer, so read-write can take place safely
// at the same time with minimal blocking
RING_BUFFER_DEFINE(manchester_rb, MANCHESTER_BUFFER_BYTES, uint8_t, uint8_t);
static manchester_rb_t rb;

// Progress along the individual byte
static uint8_t rxbuffer_writebit;

static volatile uint8_t overflow = 0;

volatile uint8_t manchester_idle_events;

static bool is_idle;

static uint8_t t1_5;
static uint8_t t2;

void manchester_init()
{
	manchester_rb_init(&rb);
	rxbuffer_writebit = 0;
	is_idle           = true;
	overflow          = 0;
	manchester_hw_init();
}

static inline void manchester_handle_timer_int()
{
	// Timer expired
	is_idle = true;

	rxbuffer_writebit = 0;
	manchester_idle_events++;
}

static inline void manchester_handle_pin_change_int()
{
	if (is_idle)
	{
		is_idle = false;
		manchester_hw_start_up_counter();

		// Set time thresholds to max, as we don't know just yet the period
		t1_5 = 255;
		t2   = 255;
		return;
	}

	uint8_t cval = manchester_hw_get_counter();
	DEBUG("Interrupt with cval = %i t1_5 = %i %i\n", cval, t1_5, cval - t1_5);
	if (cval >= t1_5)
	{
		DEBUG("Skipping, %i > %i\n", cval, t1_5);
		return;
	}

	uint8_t pin = manchester_hw_get_pin_value();
	if (pin)
	{
		rb.buf[rb.write] |= (1 << rxbuffer_writebit);
	}
	else
	{
		rb.buf[rb.write] &= ~(1 << rxbuffer_writebit);
	}

	rxbuffer_writebit++;
	if (rxbuffer_writebit >= 8)
	{
		rxbuffer_writebit = 0;

		if (!manchester_rb_commit(&rb))
		{
			if (overflow < 255)
			{
				overflow++;
			}
		}
	}

	// Pre-compute the new counter values, if everything went nominally,
	// cval = t, so we can correct the clock by...
	uint8_t tnew = t2 - cval;
	if (t2 == 255)
	{
		// The period read is exactly tnew
		tnew = cval;
	}

	t2         = tnew * 2;
	uint8_t t3 = t2 + tnew;
	t1_5       = t3 / 2;

	DEBUG("Processed with %i, new t1_5 %i\n", cval, t1_5);
	manchester_hw_start_down_counter(t3);
}

uint8_t manchester_get_next_count() { return manchester_rb_count(&rb); }

uint8_t manchester_get_overflow()
{
	// Safe as uint8 is atomic
	uint8_t out = overflow;
	return out;
}

void manchester_clear_overflow()
{
	// Safe as uint8 is atomic
	overflow = 0;
}

uint8_t manchester_read_next() { return manchester_rb_get(&rb); }
