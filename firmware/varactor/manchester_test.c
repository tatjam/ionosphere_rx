// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later
//
#include "../test.h"
#include "manchester.h"
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

// Mock hardware

static uint8_t hw_counter = 0;
static bool clock_up      = false;
static bool clock_running = false;
static bool transition = 0;

static void manchester_hw_init()
{
	clock_up      = false;
	clock_running = false;
	hw_counter    = 0;
	transition    = false;
}

static void manchester_hw_start_up_counter()
{
	hw_counter    = 0;
	clock_up      = true;
	clock_running = true;
}

static void manchester_hw_start_down_counter(uint8_t val)
{
	hw_counter    = val;
	clock_up      = false;
	clock_running = true;
}

static uint8_t manchester_hw_get_counter() { return hw_counter; }
static bool manchester_hw_get_pin_value() { return transition; }

static void sei() {}

static void cli() {}

#include "manchester.c"
// Test and harness helpers
int tests_run = 0;
static void test_clock_tick(uint8_t num)
{
	for (uint8_t i = 0; i < num; i++)
	{
		if (!clock_running)
		{
			return;
		}
		if (clock_up)
		{
			if (hw_counter == 255)
			{
				hw_counter    = 0;
				clock_running = false;
				manchester_handle_timer_int();
			}
			else
			{
				hw_counter++;
			}
		}
		else
		{
			if (hw_counter == 0)
			{
				hw_counter    = 255;
				clock_running = false;
				manchester_handle_timer_int();
			}
			else
			{
				hw_counter--;
			}
		}
	}
}

static void test_transition(bool dir)
{
	transition = dir;
	manchester_handle_pin_change_int();
}

static void test_send_bytes(const uint8_t *bytes, size_t n, bool *line, uint8_t h)
{
	for (size_t i = 0; i < n; i++)
	{
		for (uint8_t b = 0; b < 8; b++)
		{
			uint8_t bit = (bytes[i] >> b) & 1;
			for (uint8_t half = 0; half < 2; half++)
			{
				bool level = ((half == 0) ^ bit) != 0;
				if (level != *line)
				{
					*line = level;
					test_transition(level);
				}
				test_clock_tick(h);
			}
		}
	}
}

static char *check_real_symbol(bool start_high)
{
	manchester_init();
	bool line      = start_high;
	transition     = start_high;
	uint8_t msg[3] = {0xAA, 0x12, 0x34};
	if (start_high)
	{
		msg[0] = 0x55;
	}
	test_send_bytes(msg, 3, &line, 40);

	mu_assert("3 bytes received", manchester_get_next_count() == 3);
	mu_assert("sync byte", manchester_read_next() == msg[0]);
	mu_assert("MSB byte", manchester_read_next() == msg[1]);
	mu_assert("LSB byte", manchester_read_next() == msg[2]);
	return 0;
}

static char *test_real_start_high() { return check_real_symbol(true); }
static char *test_real_start_low() { return check_real_symbol(false); }

static char *test_init()
{
	manchester_init();
	mu_assert("get_next is 0 at start", manchester_get_next_count() == 0);
	mu_assert("get_overflow is 0 at start", manchester_get_overflow() == 0);
	return 0;
}

static char *test_nominal_read()
{
	manchester_init();

	mu_assert("is_idle is true before first change", is_idle == true);
	manchester_handle_pin_change_int();
	mu_assert("is_idle is false after first change", is_idle == false);

	const uint8_t msg[16] = {1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 1, 1};

	for (uint8_t clock = 0; clock < 32; clock++)
	{
		uint8_t clockval = clock % 2;
		uint8_t data     = msg[clock / 2];
		test_clock_tick(10);
		test_transition(data ^ clockval);
	}

	mu_assert("is_idle is false after bits", is_idle == false);
	mu_assert("2 bytes are available after bits", manchester_get_next_count() == 2);
	// Note, each byte is reversed relative to its reception, as we write the bits from LSB to MSB

	mu_assert("first byte match expected", manchester_read_next() == 0b10101101);
	mu_assert("second byte match expected", manchester_read_next() == 0b11110101);
	mu_assert("no more reads available", manchester_get_next_count() == 0);

	// Wait for clock expiry
	test_clock_tick(255);
	mu_assert("is_idle is false after clock expirty", is_idle == true);

	return 0;
}

// We allow the clock to go down to 80 ticks per transition safely
static char *test_slow_clock()
{
	manchester_init();
	manchester_handle_pin_change_int();

	const uint8_t msg[16] = {1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 1, 1};

	for (uint8_t clock = 0; clock < 32; clock++)
	{
		uint8_t clockval = clock % 2;
		uint8_t data     = msg[clock / 2];
		test_clock_tick(80);
		test_transition(data ^ clockval);
	}

	mu_assert("2 bytes are available after bits", manchester_get_next_count() == 2);
	mu_assert("first byte match expected", manchester_read_next() == 0b10101101);
	mu_assert("second byte match expected", manchester_read_next() == 0b11110101);
	mu_assert("no more reads available", manchester_get_next_count() == 0);

	return 0;
}

// We allow the clock to change at most +-10% from its nominal period without losing capture
static char *test_fiddly_clock()
{
	manchester_init();
	manchester_handle_pin_change_int();

	const uint8_t msg[16] = {1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 1, 1};

	uint8_t lastclk = 40;
	for (uint8_t clock = 0; clock < 32; clock++)
	{
		uint8_t clockval = clock % 2;
		uint8_t data     = msg[clock / 2];
		test_clock_tick(lastclk);
		lastclk = 40 + (rand() % 10 - 5);
		test_transition(data ^ clockval);
	}

	mu_assert("2 bytes are available after bits", manchester_get_next_count() == 2);
	mu_assert("first byte match expected", manchester_read_next() == 0b10101101);
	mu_assert("second byte match expected", manchester_read_next() == 0b11110101);
	mu_assert("no more reads available", manchester_get_next_count() == 0);

	return 0;
}

static char *tests()
{
	mu_run_test(test_init);
	mu_run_test(test_nominal_read);
	mu_run_test(test_slow_clock);
	mu_run_test(test_fiddly_clock);
	mu_run_test(test_real_start_high);
	mu_run_test(test_real_start_low);

	return 0;
}

int main(void)
{
	srand(12345);
	char *result = tests();
	mu_suite(result);
}
