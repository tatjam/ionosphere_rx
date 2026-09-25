// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later

#include "manchester.h"
#include <stdbool.h>

#ifndef ENV_TEST
#include <avr/interrupt.h>
void manchester_hw_init() {}
void manchester_hw_start_up_counter() {}
void manchester_hw_start_down_counter(uint8_t start) {}
uint8_t manchester_hw_get_counter() { return 0; }
uint8_t manchester_hw_get_pin_value() { return 0; }
#define DEBUG(...)
#else
#define DEBUG(...) printf(__VA_ARGS__)
#endif

// This is a ring-buffer, so read-write can take place safely
// at the same time with minimal blocking
volatile uint8_t manchester_buffer[MANCHESTER_BUFFER_BYTES];

// Next byte is read from this position in rxbuffer
static volatile uint8_t rxbuffer_readpos;
// Next byte is written to this position in rxbuffer
static volatile uint8_t rxbuffer_writepos;
// Progress along the individual byte
static uint8_t rxbuffer_writebit;

static volatile uint8_t overflow = 0;

volatile uint8_t manchester_idle_events;

static bool is_idle;

static uint8_t t1_5;
static uint8_t t2;

void manchester_init()
{
	rxbuffer_writepos = 0;
	rxbuffer_readpos  = 0;
	rxbuffer_writebit = 0;
	is_idle			  = true;
	overflow		  = 0;
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
		t2	 = 255;
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
		manchester_buffer[rxbuffer_writepos] |= (1 << rxbuffer_writebit);
	}
	else
	{
		manchester_buffer[rxbuffer_writepos] &= ~(1 << rxbuffer_writebit);
	}

	rxbuffer_writebit++;
	if (rxbuffer_writebit >= 8)
	{
		rxbuffer_writebit = 0;

		uint8_t next = rxbuffer_writepos + 1;
		if (next >= MANCHESTER_BUFFER_BYTES)
		{
			next = 0;
		}

		if (next == rxbuffer_readpos)
		{
			if (overflow < 255)
			{
				overflow++;
			}
		}
		else
		{
			rxbuffer_writepos = next;
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

	t2		   = tnew * 2;
	uint8_t t3 = t2 + tnew;
	t1_5	   = t3 / 2;

	DEBUG("Processed with %i, new t1_5 %i\n", cval, t1_5);
	manchester_hw_start_down_counter(t3);
}

uint8_t manchester_get_next_count()
{
	cli();
	uint8_t out;
	if (rxbuffer_readpos == rxbuffer_writepos)
	{
		out = 0;
	}
	else if (rxbuffer_readpos < rxbuffer_writepos)
	{
		// ..R------W.....
		out = rxbuffer_writepos - rxbuffer_readpos;
	}
	else
	{
		// --W......R----
		out = MANCHESTER_BUFFER_BYTES - rxbuffer_readpos + rxbuffer_writepos;
	}
	sei();
	return out;
}

uint8_t manchester_get_overflow()
{
	cli();
	uint8_t out = overflow;
	sei();
	return out;
}

void manchester_clear_overflow()
{
	cli();
	overflow = 0;
	sei();
}

uint8_t manchester_read_next()
{
	uint8_t out = manchester_buffer[rxbuffer_readpos];

	cli();
	rxbuffer_readpos++;
	if (rxbuffer_readpos == MANCHESTER_BUFFER_BYTES)
	{
		rxbuffer_readpos = 0;
	}
	sei();

	return out;
}
