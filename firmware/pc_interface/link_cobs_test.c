// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later
//
#include "../test.h"
#include "command.h"
#include "link.h"
#include <string.h>

#define F_CPU 16000000

// UART mock, fed with raw wire bytes
static uint8_t rx_buffer[256];
static uint8_t rx_len = 0;
static uint8_t rx_pos = 0;

uint8_t uart_get_next_count() { return rx_len - rx_pos; }

uint8_t uart_read_next() { return rx_buffer[rx_pos++]; }
void uart_tx(uint8_t byte) {}

// link_hw mocks, link.c only defines them for the AVR build
void link_hw_init() {}

void link_hw_set_fsk(uint16_t v) {}

void link_hw_toggle_off_fsk() {}

void link_hw_toggle_on_fsk() {}

// Command dispatcher mock
static uint8_t last_cmd[128];
static uint8_t last_cmd_len = 0;
static uint8_t handle_calls = 0;
static bool handle_result   = true;

bool command_decode(uint8_t *data, uint8_t size, command_t *out)
{
	memcpy(last_cmd, data, size);
	last_cmd_len = size;
	handle_calls++;

	out->kind = COMMAND_NO_OP;

	return handle_result;
}

#include "cobs.c"
#include "link.c"

static void reset()
{
	rx_len        = 0;
	rx_pos        = 0;
	handle_calls  = 0;
	last_cmd_len  = 0;
	handle_result = true;
	link_init();
}

static void feed(const uint8_t *bytes, uint8_t len)
{
	for (uint8_t i = 0; i < len; i++)
	{
		rx_buffer[rx_len++] = bytes[i];
	}
}

static void drain()
{
	do
	{
		link_task();
	} while (uart_get_next_count() > 0);
}

static char *test_valid_packet()
{
	reset();
	const uint8_t wire[] = {0x00, 0x03, 'A', 'B', 0x00};
	feed(wire, sizeof(wire));
	drain();

	mu_assert("one command handled", handle_calls == 1);
	mu_assert("command length is 2", last_cmd_len == 2);
	mu_assert("command content is AB", last_cmd[0] == 'A' && last_cmd[1] == 'B');
	return 0;
}

static char *test_packet_with_zero()
{
	reset();
	// Payload A 00 B, the embedded zero is COBS-stuffed away
	const uint8_t wire[] = {0x00, 0x02, 'A', 0x02, 'B', 0x00};
	feed(wire, sizeof(wire));
	drain();

	mu_assert("one command handled", handle_calls == 1);
	mu_assert("command length is 3", last_cmd_len == 3);
	mu_assert("zero restored in the middle",
	          last_cmd[0] == 'A' && last_cmd[1] == 0x00 && last_cmd[2] == 'B');
	return 0;
}

static char *test_garbage_before_delimiter()
{
	reset();
	const uint8_t wire[] = {0xAA, 0xBB, 0x00, 0x02, 'A', 0x00};
	feed(wire, sizeof(wire));
	drain();

	mu_assert("garbage skipped, one command handled", handle_calls == 1);
	mu_assert("command content is A", last_cmd_len == 1 && last_cmd[0] == 'A');
	return 0;
}

static char *test_rejected_packet_resync()
{
	reset();
	handle_result        = false;
	const uint8_t wire[] = {0x00, 0x02, 'A', 0x00, 0x02, 'B', 0x00};
	feed(wire, sizeof(wire));
	drain();

	mu_assert("both packets delivered despite rejection", handle_calls == 2);
	mu_assert("second packet decoded after resync", last_cmd_len == 1 && last_cmd[0] == 'B');
	return 0;
}

static char *test_oversized_code_aborts()
{
	reset();
	// 0xFF is an impossible code byte for our buffer size, the packet must be
	// dropped and the decoder must recover on the next delimiter
	const uint8_t wire[] = {0x00, 0xFF, 'A', 'B', 0x00, 0x02, 'C', 0x00};
	feed(wire, sizeof(wire));
	drain();

	mu_assert("only the good packet handled", handle_calls == 1);
	mu_assert("recovered after oversized code", last_cmd_len == 1 && last_cmd[0] == 'C');
	return 0;
}

static char *tests()
{
	mu_run_test(test_valid_packet);
	mu_run_test(test_packet_with_zero);
	mu_run_test(test_garbage_before_delimiter);
	mu_run_test(test_rejected_packet_resync);
	mu_run_test(test_oversized_code_aborts);
	return 0;
}

int tests_run = 0;
int main(void)
{
	char *result = tests();
	mu_suite(result);
}
