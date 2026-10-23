// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pot.h"
#include <util/delay.h>

#ifndef ENV_TEST

#include <avr/io.h>
#define POT_PIN_UD  PA7
#define POT_PORT_UD PORTA
#define POT_DDR_UD  DDRA

#define POT_PIN_CS  PB2
#define POT_PORT_CS PORTB
#define POT_DDR_CS  DDRB

#define POT_MAX_TAP   0x3F
#define POT_START_TAP 0x1F

#endif

// In practice 0.5 may be enough, we go for 1 just to be safe
#define T_SLEEP_US 1

static uint8_t pot_pos;

static void pot_up(uint8_t num)
{
	// Ensure U/~{D} is high
	POT_PORT_UD |= 1 << POT_PIN_UD;
	_delay_us(T_SLEEP_US); // tLUC
	// Pull ~{CS} down
	POT_PORT_CS &= ~(1 << POT_PIN_CS);
	_delay_us(T_SLEEP_US); // tLUCF
	for (uint8_t i = 0; i < num; i++)
	{
		// Move U/~{D} low
		POT_PORT_UD &= ~(1 << POT_PIN_UD);
		_delay_us(T_SLEEP_US); // tLO
		// Move U/~{D} high (this triggers an up-count)
		POT_PORT_UD |= 1 << POT_PIN_UD;
		_delay_us(T_SLEEP_US); // tHI
	}
	// Pull ~{CS} high
	POT_PORT_CS |= 1 << POT_PIN_CS;
	_delay_us(T_SLEEP_US); // tCSHI
}

static void pot_down(uint8_t num)
{
	// Ensure U/~{D} is low
	POT_PORT_UD &= ~(1 << POT_PIN_UD);
	_delay_us(T_SLEEP_US); // tLUC
	// Pull ~{CS} down
	POT_PORT_CS &= ~(1 << POT_PIN_CS);
	_delay_us(T_SLEEP_US); // tLUCF
	for (uint8_t i = 0; i < num; i++)
	{
		// Move U/~{D} high
		POT_PORT_UD |= 1 << POT_PIN_UD;
		_delay_us(T_SLEEP_US); // tHI
		// Move U/~{D} low (this triggers an down-count)
		POT_PORT_UD &= ~(1 << POT_PIN_UD);
		_delay_us(T_SLEEP_US); // tLO
	}
	// Pull ~{CS} high
	POT_PORT_CS |= 1 << POT_PIN_CS;
	_delay_us(T_SLEEP_US); // tCSHI
}

void pot_init()
{
	POT_DDR_CS |= (1 << POT_PIN_CS);
	POT_DDR_UD |= (1 << POT_PIN_UD);

	// Pull ~{CS} high
	POT_PORT_CS |= 1 << POT_PIN_CS;

	// Pull U/~{D} low
	POT_PORT_UD &= ~(1 << POT_PIN_UD);

	// ensure the pot is at its max value, with some extra pulses
	// for good measure
	pot_up(POT_MAX_TAP + 10);
	pot_pos = POT_MAX_TAP;

	pot_set_tap(POT_START_TAP);
}

void pot_set_tap(uint8_t value)
{
	if (value > POT_MAX_TAP)
	{
		value = POT_MAX_TAP;
	}

	if (value < pot_pos)
	{
		pot_down(pot_pos - value);
	}
	else if (value > pot_pos)
	{
		pot_up(value - pot_pos);
	}
	pot_pos = value;
}

uint8_t pot_get_tap() { return pot_pos; }
