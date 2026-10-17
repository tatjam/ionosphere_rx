// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later
//
#include "manchester.h"
#include "../test.h"
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

// Mock hardware

static uint8_t clock	  = 0;
static bool clock_up	  = false;
static uint8_t transition = 0;

static void manchester_hw_init()
{
	clock_up   = false;
	clock	   = 0;
	transition = 0;
}
static void manchester_hw_start_up_counter() { clock_up = true; }
static void manchester_hw_start_down_counter(uint8_t val)
{
	clock	 = val;
	clock_up = false;
}
static uint8_t manchester_hw_get_counter() { return clock; }
static uint8_t manchester_hw_get_pin_value() { return transition; }

static void sei() {}

static void cli() {}

#include "manchester.c"
// Test and harness helpers
int tests_run = 0;
static void test_clock_tick(uint8_t num)
{
	for (uint8_t i = 0; i < num; i++)
	{
		if (clock_up)
		{
			if (clock == 255)
			{
				clock = 0;
				manchester_handle_timer_int();
			}
			else
			{
				clock++;
			}
		}
		else
		{
			if (clock == 0)
			{
				clock = 255;
				manchester_handle_timer_int();
			}
			else
			{
				clock--;
			}
		}
	}
}

static void test_transition(uint8_t dir)
{
	transition = dir;
	manchester_handle_pin_change_int();
}

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
		uint8_t data	 = msg[clock / 2];
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
		uint8_t data	 = msg[clock / 2];
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
		uint8_t data	 = msg[clock / 2];
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
	return 0;
}

int main(void)
{
	srand(12345);
	char *result = tests();
	mu_suite(result);
}
