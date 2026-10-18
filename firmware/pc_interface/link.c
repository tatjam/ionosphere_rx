// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later
#include "link.h"
#include "command.h"
#include "uart.h"
#include <assert.h>

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

typedef enum
{
	LINK_ALIGN,
	LINK_START,
	LINK_COLLECT,
} link_task_state_t;

static link_task_state_t state;
static bool enter_programming;

static uint8_t link_buffer[128];
static uint8_t link_ptr    = 0;
static uint8_t num_to_zero = 0;

void link_init()
{
	state = LINK_ALIGN;

	link_hw_init();
	link_hw_set_fsk(FSK_V_LOW);
	link_hw_toggle_off_fsk();
}

static command_t cmd;

// Link mode runs a simple COBS with 0 as the special value, for simplicity
// we just skip messages until the first zero
void link_task()
{
	while (uart_get_next_count() != 0)
	{
		if (link_ptr >= sizeof(link_buffer) || num_to_zero >= sizeof(link_buffer))
		{
			state       = LINK_ALIGN;
			link_ptr    = 0;
			num_to_zero = 0;
			continue;
		}

		if (state == LINK_ALIGN && uart_read_next() == 0)
		{
			// Found the COBS delimiter, start reading bytes!
			state    = LINK_START;
			link_ptr = 0;
			break;
		}
		else if (state == LINK_START)
		{
			// Read the COBS number of bytes to next zero
			num_to_zero = uart_read_next();
			if (num_to_zero != 0)
			{
				state = LINK_COLLECT;
			}
		}
		else if (state == LINK_COLLECT)
		{
			assert(num_to_zero > 0);
			num_to_zero--;
			if (num_to_zero == 0)
			{
				num_to_zero = uart_read_next();
				if (num_to_zero == 0)
				{
					// Packet completed
					state      = LINK_START;
					bool valid = command_decode(link_buffer, link_ptr, &cmd);
					if (valid)
					{
						link_handle_command(&cmd);
					}

					link_ptr = 0;
				}
				else
				{
					// Insert a zero
					link_buffer[link_ptr] = 0;
					link_ptr++;
				}
			}
			else
			{
				link_buffer[link_ptr] = uart_read_next();
				link_ptr++;
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
