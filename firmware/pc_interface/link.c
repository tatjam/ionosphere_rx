// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later
#include "link.h"
#include "../shared/ring_buffer.h"
#include "cobs.h"
#include "command.h"
#include "uart.h"

#ifndef ENV_TEST
#include <avr/interrupt.h>
#include <avr/io.h>

void link_hw_init()
{
	// Set the pin to output and the timer to output on it
	DDRD |= _BV(PD3);

	OCR2B = 0;

	// Set WGM = 2, CTC with TOP = OCR2A, update of OCR2A is immediate,
	// toggling the output pin on compare match
	TCCR2A = _BV(WGM21) | _BV(COM2B0);
}

// Sets the FSK frequency to F_CPU / (2 * (1 + v))
void link_hw_set_fsk(uint16_t v)
{
	OCR2A = v;
	// Restart the counter to prevent long-symbol, this does break phase continuity
	TCNT2 = 0;
}

void link_hw_toggle_off_fsk()
{
	// Set clock source = none
	TCCR2B = 0;
}

void link_hw_toggle_on_fsk()
{
	// Restart counter
	TCNT2 = 0;
	// Set clock source = I/O clock
	TCCR2B = _BV(CS20);
}

#define DEBUG(...)
#else

#define DEBUG(...) printf(__VA_ARGS__)

#endif

#define SOLVE_FOR_V(f) ((F_CPU + (f)) / (2 * (f)) - 1)
#define FSK_V_HIGH     SOLVE_FOR_V(LINK_FSK_HIGH)
#define FSK_V_LOW      SOLVE_FOR_V(LINK_FSK_LOW)

RING_BUFFER_DEFINE(sequence_rb, SEQUENCE_RB_SIZE, uint8_t, uint16_t);
sequence_rb_t sequence_rb;

static cobs_decoder_t decoder;
static uint8_t link_buffer[sizeof(command_t) + 3];
static command_t cmd;
static bool enter_programming;

void link_init()
{
	cobs_init(&decoder, link_buffer, sizeof(link_buffer));

	link_hw_init();
	link_hw_set_fsk(FSK_V_LOW);
	link_hw_toggle_off_fsk();
}

// Link mode runs a simple COBS with 0 as the special value, for simplicity
// we just skip messages until the first zero
void link_task()
{
	while (uart_get_next_count() != 0)
	{
		uint8_t len;
		if (cobs_feed(&decoder, uart_read_next(), &len))
		{
			if (command_decode(link_buffer, len, &cmd))
			{
				link_handle_command(&cmd);
			}
		}
	}
}

void link_handle_command(const command_t *cmd)
{
	if (cmd->kind == COMMAND_ENTER_PROGRAMMER)
	{
		enter_programming = true;
		uart_tx('G');
	}
}

bool link_wants_enter_programming()
{
	bool out          = enter_programming;
	enter_programming = false;
	return out;
}
