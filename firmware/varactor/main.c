// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../shared/defines.h"
#include <avr/builtins.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdbool.h>

#include <util/delay.h>

#include "dac_spi.h"
#include "manchester.h"
#include "pot.h"

#define RELAY_PORT PORTA
#define RELAY_DDR  DDRA
#define RELAY1_PIN PA0
#define RELAY2_PIN PA1
#define RELAY3_PIN PA2

// Returns 0 if the byte contains an even number of 1s, 1 if it contains an odd number, i.e.
// the XOR of all bits in byte
static inline uint8_t compute_parity(uint8_t byte)
{
	// byte    = [a][b][c][d][e][f][g][h]
	// swapped = [e][f][g][h][a][b][c][d]
	uint8_t swapped = __builtin_avr_swap(byte);

	// byte = ____ [a^e][b^f][c^g][h^d]
	byte ^= swapped;

	// swapped = ______ [a^e][b^f]
	swapped = byte >> 2;

	// byte = ______ [c^g^a^e][h^d^b^f]
	byte ^= swapped;

	// swapped = _______ [c^g^a^e]
	swapped = byte >> 1;

	// byte = _______[h^d^b^f^c^g^a^e]
	byte ^= swapped;

	// byte = 0000000[h^d^b^f^c^g^a^e]
	byte &= 1;

	return byte;
}

static inline void nominal_process(uint8_t high, uint8_t low, bool parity_good)
{
	if (!parity_good)
	{
		return;
	}

	// Relays states from three bits after parity bit of high byte:
	// PB R2 R1 R0 DB DA D9 D8 | D7 D6 D5 D4 D3 D2 D1 D0
	uint8_t relays = __builtin_avr_swap(high) & 0x0F;
	if (relays & 0x01)
	{
		RELAY_PORT |= 1 << RELAY1_PIN;
	}
	else
	{
		RELAY_PORT &= ~(1 << RELAY1_PIN);
	}

	if (relays & 0x02)
	{
		RELAY_PORT |= 1 << RELAY2_PIN;
	}
	else
	{
		RELAY_PORT &= ~(1 << RELAY2_PIN);
	}

	if (relays & 0x04)
	{
		RELAY_PORT |= 1 << RELAY3_PIN;
	}
	else
	{

		RELAY_PORT &= ~(1 << RELAY3_PIN);
	}

	// Tuning from 12 remaining bits, 4 from high byte, 8 from low byte
	uint16_t tuning = low;
	tuning |= ((uint16_t) (high & 0xF) << 8);
	dac_spi_send(tuning);
}

int main()
{
	// Set clock pre-scaler to 1 for max speed operation
	cli();
	CLKPR = 0b10000000;
	CLKPR = 0b00000000;
	sei();

	RELAY_DDR |= (1 << RELAY1_PIN);
	RELAY_DDR |= (1 << RELAY2_PIN);
	RELAY_DDR |= (1 << RELAY3_PIN);

	RELAY_PORT &= ~(1 << RELAY1_PIN);
	RELAY_PORT &= ~(1 << RELAY2_PIN);
	RELAY_PORT &= ~(1 << RELAY3_PIN);

	dac_spi_init();
	manchester_init();
	pot_init();

	// This works well for 100kHz <-> 300kHz
	pot_set_tap(57);

	bool synced       = false;
	uint8_t last_idle = 0;

	while (1)
	{
		uint8_t idle = manchester_idle_events;
		if (idle != last_idle)
		{
			// Line is idle
			last_idle = idle;
			synced    = false;
		}

		if (manchester_get_overflow())
		{
			synced = false;

			// Clear the buffer if any
			manchester_clear_overflow();
			while (manchester_get_next_count() > 0)
			{
				manchester_read_next();
			}
			continue;
		}

		if (!synced)
		{
			// Wait for sync word
			if (manchester_get_next_count() < 1)
			{
				continue;
			}

			if (manchester_read_next() == MANCHESTER_SYNC_WORD)
			{
				// We found the sync word, start a frame!
				// Note, if the sync word is not found, we just skip the byte
				synced = true;
			}

			continue;
		}

		if (manchester_get_next_count() < 2)
		{
			continue;
		}

		uint8_t high = manchester_read_next();
		uint8_t low  = manchester_read_next();

		// We always expect parity bit to be 0
		bool parity_correct = (compute_parity(low) ^ compute_parity(high)) == 0;

		nominal_process(high, low, parity_correct);
	}

	return 0;
}
