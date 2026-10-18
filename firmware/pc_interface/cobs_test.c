// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later
//
#include "../test.h"
#include "cobs.h"
#include <string.h>

#include "cobs.c"

static uint8_t dec_buf[128];
static cobs_decoder_t dec;

// Last completed packet, captured by feed()
static uint8_t last_packet[128];
static uint8_t last_len     = 0;
static uint8_t packet_count = 0;

static void reset()
{
	cobs_init(&dec, dec_buf, sizeof(dec_buf));
	last_len     = 0;
	packet_count = 0;
}

static void feed(const uint8_t *bytes, uint8_t len)
{
	for (uint8_t i = 0; i < len; i++)
	{
		uint8_t n;
		if (cobs_feed(&dec, bytes[i], &n))
		{
			memcpy(last_packet, dec.buffer, n);
			last_len = n;
			packet_count++;
		}
	}
}

static void feed_byte(uint8_t byte) { feed(&byte, 1); }

static char *test_valid_packet()
{
	reset();
	const uint8_t wire[] = {0x00, 0x03, 'A', 'B', 0x00};
	feed(wire, sizeof(wire));

	mu_assert("one packet delivered", packet_count == 1);
	mu_assert("packet length is 2", last_len == 2);
	mu_assert("packet content is AB", last_packet[0] == 'A' && last_packet[1] == 'B');
	return 0;
}

static char *test_packet_with_zero()
{
	reset();
	// Payload A 00 B, the embedded zero is COBS-stuffed away
	const uint8_t wire[] = {0x00, 0x02, 'A', 0x02, 'B', 0x00};
	feed(wire, sizeof(wire));

	mu_assert("one packet delivered", packet_count == 1);
	mu_assert("packet length is 3", last_len == 3);
	mu_assert("zero restored in the middle",
	          last_packet[0] == 'A' && last_packet[1] == 0x00 && last_packet[2] == 'B');
	return 0;
}

static char *test_garbage_before_delimiter()
{
	reset();
	const uint8_t wire[] = {0xAA, 0xBB, 0x00, 0x02, 'A', 0x00};
	feed(wire, sizeof(wire));

	mu_assert("garbage skipped, one packet delivered", packet_count == 1);
	mu_assert("packet content is A", last_len == 1 && last_packet[0] == 'A');
	return 0;
}

static char *test_oversized_code_aborts()
{
	reset();
	// 0xFF is an impossible code byte for our buffer size, the packet must be
	// dropped and the decoder must recover on the next delimiter
	const uint8_t wire[] = {0x00, 0xFF, 'A', 'B', 0x00, 0x02, 'C', 0x00};
	feed(wire, sizeof(wire));

	mu_assert("only the good packet delivered", packet_count == 1);
	mu_assert("recovered after oversized code", last_len == 1 && last_packet[0] == 'C');
	return 0;
}

static char *test_buffer_exhaustion_aborts()
{
	reset();
	// A packet longer than the decoder buffer is dropped mid-flight and the
	// decoder resynchronizes on the next delimiter
	feed_byte(0x00);
	feed_byte(0x7F);
	for (uint8_t i = 0; i < 126; i++)
	{
		feed_byte('A');
	}
	feed_byte(0x7F);
	for (uint8_t i = 0; i < 126; i++)
	{
		feed_byte('B');
	}
	feed_byte(0x00);
	mu_assert("oversized packet not delivered", packet_count == 0);

	const uint8_t wire[] = {0x02, 'C', 0x00};
	feed(wire, sizeof(wire));
	mu_assert("recovered on the next packet",
	          packet_count == 1 && last_len == 1 && last_packet[0] == 'C');
	return 0;
}

static char *test_empty_packets()
{
	reset();
	const uint8_t double_delimiter[] = {0x00, 0x00};
	feed(double_delimiter, 2);
	mu_assert("empty packet not delivered", packet_count == 0);

	const uint8_t zero_len[] = {0x00, 0x01, 0x00};
	feed(zero_len, 3);
	mu_assert("zero-length packet delivered", packet_count == 1 && last_len == 0);
	return 0;
}

static char *tests()
{
	mu_run_test(test_valid_packet);
	mu_run_test(test_packet_with_zero);
	mu_run_test(test_garbage_before_delimiter);
	mu_run_test(test_oversized_code_aborts);
	mu_run_test(test_buffer_exhaustion_aborts);
	mu_run_test(test_empty_packets);
	return 0;
}

int tests_run = 0;
int main(void)
{
	char *result = tests();
	mu_suite(result);
}
