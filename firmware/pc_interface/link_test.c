// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later
//
#include "../test.h"
#include "command.h"
#include "link.h"
#include "timebase.h"

#define F_CPU 16000000

static bool symbol_timer_running = false;

static uint8_t fsk_vals[1024];
static size_t fsk_ptr = 0;

// uart mock
static uint8_t uart_tx_buf[256];
static size_t uart_tx_ptr = 0;

void reset()
{
	uart_tx_ptr          = 0;
	fsk_ptr              = 0;
	symbol_timer_running = false;

	link_init();
}

void uart_tx(uint8_t data)
{
	uart_tx_buf[uart_tx_ptr] = data;
	uart_tx_ptr++;
}

// fsk mock
void link_hw_init() {}

void link_hw_set_fsk(uint8_t v)
{
	fsk_vals[fsk_ptr] = v;
	fsk_ptr++;
}

void link_hw_toggle_off_fsk() {}

void link_hw_launch_bit_clock() { symbol_timer_running = true; }

void link_hw_stop_bit_clock() {}

#include "link.c"

// mock timebase

void timebase_stop_slot() {}

bool timebase_start_slot(uint32_t start_sec, uint16_t rate) { return true; }

static uint32_t second = 0;
static uint16_t tick   = 0;

uint32_t timebase_get_second() { return second; }

uint16_t timebase_get_tick() { return tick; }

static char *test_no_op()
{
	reset();
	command_t cmd;
	cmd.kind = COMMAND_NO_OP;
	link_handle_command(&cmd);

	mu_assert("single byte reply", uart_tx_ptr == 1);
	mu_assert("single byte is LINK_GOOD_BYTE", uart_tx_buf[0] == LINK_GOOD_BYTE);

	return 0;
}

static char *test_enter_programmer()
{
	reset();

	mu_assert("Link wants programming is false at start", !link_wants_enter_programming());

	command_t cmd;
	cmd.kind = COMMAND_ENTER_PROGRAMMER;
	link_handle_command(&cmd);

	mu_assert("single byte reply", uart_tx_ptr == 1);
	mu_assert("single byte is LINK_GOOD_BYTE", uart_tx_buf[0] == LINK_GOOD_BYTE);
	mu_assert("Link wants programming is true", link_wants_enter_programming());

	return 0;
}

static char *test_sequence_append_fails_if_not_running()
{
	reset();
	command_t cmd;
	cmd.kind                                = COMMAND_APPEND_SEQUENCE;
	cmd.command_append_sequence.num_symbols = 2;
	cmd.command_append_sequence.symbols[0]  = 10;
	cmd.command_append_sequence.symbols[1]  = 20;

	link_handle_command(&cmd);
	mu_assert("2 byte reply", uart_tx_ptr == 2);
	mu_assert("first byte is LINK_GOOD_BYTE", uart_tx_buf[0] == LINK_GOOD_BYTE);
	mu_assert("zero queued reply", uart_tx_buf[1] == 0);

	mu_assert("zero_queued", sequence_rb_empty(&sequence_rb));

	return 0;
}

static char *test_sequence_start_and_append()
{
	reset();
	command_t cmd;
	cmd.kind                                         = COMMAND_START_SEQUENCE;
	cmd.command_start_sequence.starting_second       = 10;
	cmd.command_start_sequence.ticks_between_symbols = 1000;
	cmd.command_start_sequence.num_symbols           = 2;
	cmd.command_start_sequence.symbols[0]            = 10;
	cmd.command_start_sequence.symbols[1]            = 20;

	link_handle_command(&cmd);
	mu_assert("2 byte reply", uart_tx_ptr == 2);
	mu_assert("first byte is LINK_GOOD_BYTE", uart_tx_buf[0] == LINK_GOOD_BYTE);
	mu_assert("2 queued reply", uart_tx_buf[1] == 2);

	mu_assert("two queued", sequence_rb_count(&sequence_rb) == 2);

	cmd.kind                                = COMMAND_APPEND_SEQUENCE;
	cmd.command_append_sequence.num_symbols = 2;
	cmd.command_append_sequence.symbols[0]  = 30;
	cmd.command_append_sequence.symbols[1]  = 40;

	uart_tx_ptr = 0;
	link_handle_command(&cmd);
	mu_assert("2 byte reply", uart_tx_ptr == 2);
	mu_assert("first byte is LINK_GOOD_BYTE", uart_tx_buf[0] == LINK_GOOD_BYTE);
	mu_assert("2 queued reply", uart_tx_buf[1] == 2);

	mu_assert("four queued", sequence_rb_count(&sequence_rb) == 4);

	mu_assert("first queued is 10", sequence_rb_get(&sequence_rb) == 10);
	mu_assert("second queued is 20", sequence_rb_get(&sequence_rb) == 20);
	mu_assert("third queued is 30", sequence_rb_get(&sequence_rb) == 30);
	mu_assert("fourth queued is 40", sequence_rb_get(&sequence_rb) == 40);

	return 0;
}

static char *test_get_clock()
{
	reset();
	second = 0xDEADBEEF;
	tick   = 0xC0FF;

	command_t cmd;
	cmd.kind = COMMAND_GET_CLOCK;
	link_handle_command(&cmd);

	mu_assert("7 byte reply", uart_tx_ptr == 7);
	mu_assert("first byte is LINK_GOOD_BYTE", uart_tx_buf[0] == LINK_GOOD_BYTE);
	mu_assert("second byte is tick's LSB", uart_tx_buf[1] == 0xFF);
	mu_assert("third byte is tick's MSB", uart_tx_buf[2] == 0xC0);
	mu_assert("fourth byte is second's LSB", uart_tx_buf[3] == 0xEF);
	mu_assert("fifth byte is second's second LSB", uart_tx_buf[4] == 0xBE);
	mu_assert("sixth byte is second's second MSB", uart_tx_buf[5] == 0xAD);
	mu_assert("seventh byte is second's MSB", uart_tx_buf[6] == 0xDE);

	return 0;
}

static uint16_t get_remaining_in_queue()
{
	command_t cmd;
	cmd.kind = COMMAND_QUERY_SYMBOL_QUEUE;

	size_t last = uart_tx_ptr;
	link_handle_command(&cmd);
	uint16_t out = uart_tx_buf[last + 1] & 0x00FF;
	out |= ((uint16_t)uart_tx_buf[last + 2] << 8) & 0xFF00;

	return out;
}

static bool check_fsk_byte(uint8_t expected, size_t offset)
{
	// Assume line is low before start
	bool last_state_high = false;
	if (offset != 0)
	{
		last_state_high = fsk_vals[offset - 1] == FSK_V_HIGH;
	}

	for (size_t i = 0; i < 16; i++)
	{
		bool new_state = fsk_vals[offset + i] == FSK_V_HIGH;
		if (i % 2 == 0)
		{
			bool expected_high = (expected >> (i / 2) & 0x1) != 0;
			if (new_state == last_state_high)
			{
				// Error, no transition found
				return false;
			}
			else if (last_state_high && expected_high)
			{
				// Transition should have been low -> high
				return false;
			}
			else if (!last_state_high && !expected_high)
			{
				// Transition should have been high -> low
				return false;
			}
		}
		last_state_high = new_state;
	}
	return true;
}

static char *test_sequencing()
{
	reset();
	second = 0xDEADBEEF;
	tick   = 0xC0FF;

	command_t cmd;
	cmd.kind                                         = COMMAND_START_SEQUENCE;
	cmd.command_start_sequence.starting_second       = 10;
	cmd.command_start_sequence.ticks_between_symbols = 1000;
	cmd.command_start_sequence.num_symbols           = 2;
	cmd.command_start_sequence.symbols[0]            = 0x1234;
	cmd.command_start_sequence.symbols[1]            = 0x5678;

	link_handle_command(&cmd);

	mu_assert("before tick 2 remain", get_remaining_in_queue() == 2);
	mu_assert("before tick fsk set to 0", fsk_ptr == 1);
	mu_assert("before tick fsk set to 0", fsk_vals[0] == FSK_V_LOW);

	timebase_on_slot(0);
	mu_assert("after first tick 1 remains", get_remaining_in_queue() == 1);
	mu_assert("after first tick, symbol timer launched", symbol_timer_running);
	mu_assert("after first tick , first half-bit sent", fsk_ptr == 2);

	// Simulate ISR firing
	for (size_t i = 0; i < 47; i++)
	{
		emit_halfbit();
	}

	// 48 transitions + the starting zero
	mu_assert("after 47 timer ticks, entire first symbol has been sent", fsk_ptr == 49);

	// Sync = MANCHESTER_SYNC_WORD
	mu_assert("Sync byte", check_fsk_byte(MANCHESTER_SYNC_WORD, 2));
	// First byte (MSB)
	mu_assert("MSB", check_fsk_byte(0x12, 18));
	// Second byte (LSB)
	mu_assert("LSB", check_fsk_byte(0x34, 34));

	timebase_on_slot(0);
	mu_assert("after second tick 0 remains", get_remaining_in_queue() == 0);
	mu_assert("after second tick, first half-bit sent", fsk_ptr == 50);

	// Simulate ISR firing
	for (size_t i = 0; i < 47; i++)
	{
		emit_halfbit();
	}

	mu_assert("after 47 timer ticks, entire second symbol has been sent", fsk_ptr == 97);

	// Sync = MANCHESTER_SYNC_WORD
	mu_assert("Sync byte", check_fsk_byte(MANCHESTER_SYNC_WORD, 50));
	// First byte (MSB)
	mu_assert("MSB", check_fsk_byte(0x56, 66));
	// Second byte (LSB)
	mu_assert("LSB", check_fsk_byte(0x78, 82));

	return 0;
}

static char *tests()
{
	mu_run_test(test_no_op);
	mu_run_test(test_enter_programmer);
	mu_run_test(test_sequence_append_fails_if_not_running);
	mu_run_test(test_sequence_start_and_append);
	mu_run_test(test_get_clock);
	mu_run_test(test_sequencing);
	return 0;
}

int tests_run = 0;
int main()
{
	char *result = tests();
	mu_suite(result);
}
