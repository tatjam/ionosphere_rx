// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later

#include "manchester.h"
#include <avr/interrupt.h>
#include <stdbool.h>

#ifdef ENV_TEST
#include "manchester_mock.inc.h"
#else
#include "manchester_hw.inc.h"
#endif

// This is a ring-buffer, so read-write can take place safely
// at the same time with minimal blocking
uint8_t manchester_buffer[MANCHESTER_BUFFER_BYTES];

// Next byte is read from this position in rxbuffer
static uint8_t rxbuffer_readpos;
// Next byte is written to this position in rxbuffer
static uint8_t rxbuffer_writepos;
// Progress along the individual byte
static uint8_t rxbuffer_writebit;

static uint8_t overflow = 0;

static bool is_idle;

static uint8_t t1_5;
static uint8_t t2;

void manchester_init()
{
	rxbuffer_writepos = 0;
	rxbuffer_readpos  = 0;
	rxbuffer_writebit = 0;
	is_idle			  = true;
	manchester_hw_init();
}

static inline void manchester_handle_timer_int()
{
	// Timer expired
	is_idle = true;

	rxbuffer_writebit = 0;
}

static inline void manchester_handle_pin_change_int()
{
	if (is_idle)
	{
		is_idle = false;
		manchester_hw_start_up_counter();
		return;
	}

	uint8_t cval = manchester_hw_get_counter();
	if (cval >= t1_5)
	{
		return;
	}

	if(overflow >= 1) {
		overflow++;
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
		rxbuffer_writepos++;

		if (rxbuffer_writepos >= MANCHESTER_BUFFER_BYTES)
		{
			rxbuffer_writepos = 0;
		}

		if (rxbuffer_writepos == rxbuffer_readpos)
		{
			overflow++;
		}
	}

	// Pre-compute the new counter values
	uint8_t tnew = t2 - cval;
	t2			 = tnew * 2;
	uint8_t t3	 = t2 + tnew;
	t1_5		 = t3 / 2;

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

uint8_t manchester_read_next()
{
	uint8_t out = manchester_buffer[rxbuffer_readpos];

	cli();
	rxbuffer_readpos++;
	if (rxbuffer_readpos == MANCHESTER_BUFFER_BYTES)
	{
		rxbuffer_readpos = 0;
	}
	overflow = 0;
	sei();

	return out;
}
