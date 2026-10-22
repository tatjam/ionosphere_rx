// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later
#include "link.h"
#include "../shared/critical_section.h"
#include "../shared/defines.h"
#include "../shared/ring_buffer.h"
#include "command.h"
#include "timebase.h"
#include "uart.h"

RING_BUFFER_DEFINE(sequence_rb, SEQUENCE_RB_SIZE, uint8_t, uint16_t);
static sequence_rb_t sequence_rb;

static link_error_t link_error;
static volatile uint8_t frame[3];
static volatile uint8_t symbol_progress;

static bool enter_programming;

#define SOLVE_FOR_V(f) ((F_CPU + (f)) / (2 * (f)) - 1)
#define FSK_V_HIGH     SOLVE_FOR_V(LINK_FSK_HIGH)
#define FSK_V_LOW      SOLVE_FOR_V(LINK_FSK_LOW)

// TODO: Sometimes we wish to skip the sync symbol!
#define HALFBITS_PER_SYMBOL 48

#ifndef ENV_TEST
#include <avr/interrupt.h>
#include <avr/io.h>

void link_hw_init()
{
	// Timer2 for the FSK

	// Set the pin to output and the timer to output on it
	DDRD |= _BV(PD3);

	OCR2B = 0;

	// Set WGM = 2, CTC with TOP = OCR2A, update of OCR2A is immediate,
	// toggling the output pin on compare match
	TCCR2A = _BV(WGM21) | _BV(COM2B0);

	// Timer0 for the bit timing, CTC (clear time on compare match) with compare to OCR0A
	TCCR0A = _BV(WGM01);
}

// Sets the FSK frequency to F_CPU / (2 * (1 + v))
void link_hw_set_fsk(uint8_t v)
{
	OCR2A = v;
	// Restart the counter to prevent long-symbol, this does break phase continuity
	TCNT2 = 0;

	// Set clock source = I/O clock, this starts the clock if it was not running
	TCCR2B = _BV(CS20);
}

void link_hw_toggle_off_fsk()
{
	// Set clock source = none
	TCCR2B = 0;
}

void link_hw_launch_bit_clock()
{
	// Just in case, this should stop the counter
	TCCR0B = 0;
	// Count up to tick 64, approx 1 tick per ms
	OCR0A = 64;
	// Restart counter
	TCNT0 = 0;
	// Enable interrupt (Clear them first)
	TIFR0 = _BV(OCF0A);
	TIMSK0 |= _BV(OCIE0A);

	// Prescaler to 256 (each tick is 16us), the clock starts now!
	TCCR0B |= _BV(CS02);
}

void link_hw_stop_bit_clock()
{
	// Reset clock source
	TCCR0B = 0;
	// Disable interrupt
	TIMSK0 &= ~_BV(OCIE0A);
}

#define DEBUG(...)
#else

#define DEBUG(...) printf(__VA_ARGS__)

#endif

static void emit_halfbit()
{
	// One bit advances every 2 ticks
	uint8_t bit_progress = symbol_progress >> 1;
	// One byte advances every 8 bits
	uint8_t byte_progress = bit_progress >> 3;

	// Last 3 bits of the bit_progress is the position within the bit
	uint8_t bit_pos = bit_progress & 0b00000111;

	// The byte of data we are sending right now
	uint8_t sending = frame[byte_progress];

	// The individual bit we are sending
	uint8_t bit = (sending >> bit_pos) & 0x1;

	// Manchester clock
	uint8_t clock = symbol_progress % 2 == 0;

	if ((clock ^ bit) != 0)
	{
		link_hw_set_fsk(FSK_V_HIGH);
	}
	else
	{
		link_hw_set_fsk(FSK_V_LOW);
	}

	symbol_progress += 1;
}

static void launch_next_symbol(uint16_t symbol)
{
	frame[0]        = MANCHESTER_SYNC_WORD;
	frame[1]        = (symbol >> 8) & 0xFF;
	frame[2]        = symbol & 0xFF;
	symbol_progress = 0;

	emit_halfbit();

	link_hw_launch_bit_clock();
}

#ifndef ENV_TEST

// Runs on eveery Timer0 tick, see adjustment of prescaler and OCR0A, one entire
// symbol is sent in 48 ticks (32 if no sync word)
ISR(TIMER0_COMPA_vect)
{
	// TODO: If we send them fast, sync word is skipped and this is 32
	if (symbol_progress >= HALFBITS_PER_SYMBOL)
	{
		link_hw_stop_bit_clock();
		return;
	}
	emit_halfbit();
}

#endif

void link_init()
{
	link_hw_init();
	link_hw_set_fsk(FSK_V_LOW);
	link_hw_toggle_off_fsk();

	enter_programming = false;
	link_error        = LINK_NO_ERROR;
	symbol_progress   = HALFBITS_PER_SYMBOL;
}

// Strong symbol override of the timebase slot function
void timebase_on_slot(uint32_t tick_num)
{
	if (symbol_progress != HALFBITS_PER_SYMBOL)
	{
		// Somehow previous sequence_next was not processed
		link_error = LINK_LINK_SLOW;
	}

	// NOTE: Called from ISR no need for atomic
	if (!sequence_rb_empty(&sequence_rb))
	{
		uint16_t sym = sequence_rb_get(&sequence_rb);
		launch_next_symbol(sym);
	}
	else
	{
		// Sequence is over, end the slots
		timebase_stop_slot();
		link_hw_toggle_off_fsk();
	}
}

void link_handle_command(const command_t *cmd)
{
	if (cmd->kind == COMMAND_NO_OP)
	{
		uart_tx(LINK_GOOD_BYTE);
	}
	else if (cmd->kind == COMMAND_ENTER_PROGRAMMER)
	{
		enter_programming = true;
		uart_tx(LINK_GOOD_BYTE);
	}
	else if (cmd->kind == COMMAND_START_SEQUENCE)
	{
		uart_tx(LINK_GOOD_BYTE);
		uint8_t sym = 0;
		for (sym = 0; sym < cmd->command_start_sequence.num_symbols; sym++)
		{
			bool success    = false;
			uint16_t symbol = cmd->command_start_sequence.symbols[sym];
			CRITICAL_SECTION { success = sequence_rb_put(&sequence_rb, symbol); }
			if (!success)
			{
				link_error = LINK_SYMBOLS_OVERFLOW;
				break;
			}
		}
		if (!timebase_start_slot(cmd->command_start_sequence.starting_second,
		                         cmd->command_start_sequence.ticks_between_symbols))
		{
			link_error = LINK_SYMBOLS_START_IN_PAST;
			sym        = 0;
			CRITICAL_SECTION { sequence_rb_drain(&sequence_rb); }
		}

		uart_tx(sym);
	}
	else if (cmd->kind == COMMAND_APPEND_SEQUENCE)
	{
		uart_tx(LINK_GOOD_BYTE);
		uint8_t sym     = 0;
		bool is_running = false;
		CRITICAL_SECTION { is_running = !sequence_rb_empty(&sequence_rb); }

		// Only add if the sequence already contains data
		if (is_running)
		{
			for (sym = 0; sym < cmd->command_append_sequence.num_symbols; sym++)
			{
				bool success    = false;
				uint16_t symbol = cmd->command_append_sequence.symbols[sym];
				CRITICAL_SECTION { success = sequence_rb_put(&sequence_rb, symbol); }
				if (!success)
				{
					link_error = LINK_SYMBOLS_OVERFLOW;
					break;
				}
			}
		}
		uart_tx(sym);
	}
	else if (cmd->kind == COMMAND_QUERY_SYMBOL_QUEUE)
	{
		uint16_t remain;
		CRITICAL_SECTION { remain = sequence_rb_count(&sequence_rb); }

		uart_tx(LINK_GOOD_BYTE);
		uart_tx(remain & 0xFF);
		uart_tx((remain >> 8) & 0xFF);
	}
	else if (cmd->kind == COMMAND_SET_CLOCK)
	{
		uart_tx(LINK_GOOD_BYTE);
	}
	else if (cmd->kind == COMMAND_GET_CLOCK)
	{
		uint32_t second = timebase_get_second();
		uint16_t tick   = timebase_get_tick();
		uart_tx(LINK_GOOD_BYTE);

		uart_tx(tick & 0xFF);
		uart_tx((tick >> 8) & 0xFF);

		uart_tx(second & 0xFF);
		uart_tx((second >> 8) & 0xFF);
		uart_tx((second >> 16) & 0xFF);
		uart_tx((second >> 24) & 0xFF);
	}
	else if (cmd->kind == COMMAND_GET_ERROR)
	{
		uart_tx(LINK_GOOD_BYTE);
		uart_tx(link_error);
		link_error = LINK_NO_ERROR;
	}
	else
	{
		uart_tx(LINK_BAD_BYTE);
	}
}

bool link_wants_enter_programming()
{
	bool out          = enter_programming;
	enter_programming = false;
	return out;
}
