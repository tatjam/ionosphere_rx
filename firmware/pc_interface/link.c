// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later
#include "link.h"
#include "uart.h"
#include <assert.h>

#ifndef ENV_TEST
#include <avr/interrupt.h>
#include <avr/io.h>

void link_hw_init()
{
	// Set the pin to output and the timer to output on it
	DDRB |= _BV(PB1);

	// We setup the "Fast PWM" mode, with TOP = OCR1A, WGM = 15
	TCCR1A |= _BV(WGM11) | _BV(WGM10);
	TCCR1B |= _BV(WGM13) | _BV(WGM12);

	// Toggle output (PB1) on compare match with OCR1A = TOP
	TCCR1A |= _BV(COM1A0);
}

// Sets the FSK frequency to F_CPU / (2 * (1 + v))
// NOTE: The change applies on next BOTTOM, so this is safe to call while
// the timer is running!
void link_hw_set_fsk(uint16_t v) { OCR1A = v; }

void link_hw_toggle_off_fsk()
{
	// Set clock source = none
	TCCR1B &= ~(_BV(CS12) | _BV(CS11) | _BV(CS10));
}

void link_hw_toggle_on_fsk()
{
	// Restart counter
	TCNT1 = 0;
	// Set clock source = I/O clock
	TCCR1B |= _BV(CS10);
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

void link_init()
{
	state = LINK_ALIGN;

	link_hw_init();
	link_hw_set_fsk(FSK_V_LOW);
	link_hw_toggle_off_fsk();
}

static uint8_t link_buffer[128];
static uint8_t link_ptr    = 0;
static uint8_t num_to_zero = 0;

// Link mode runs a simple COBS with 0 as the special value, for simplicity
// we just skip messages until the first zero
bool link_task()
{
	command_t cmd;
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

	return false;
}

void link_handle_command(const command_t *cmd)
{
	// TODO: dispatch commands
}
