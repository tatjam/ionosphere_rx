// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later
// Simple fletcher-16 checksum
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Fletcher-16 checksum over a stream of bytes. The accumulator packs both sums:
// low byte = sum1, high byte = sum2. Usage:
//   uint16_t acc = checksum_reset();
//   acc = checksum_update(acc, byte);    // per byte
//   uint16_t cks = checksum_final(acc);  // compare against the received checksum
static inline __attribute__((unused)) uint16_t checksum_reset() { return 0; }

static inline __attribute__((unused)) uint16_t checksum_update(uint16_t acc, uint8_t byte)
{
	// Sums are reduced mod 255 on every step, so packets of any length are fine.
	// Both sums stay <= 254, so one conditional subtract is enough after an add
	uint16_t sum1 = (acc & 0xFF) + byte;
	if (sum1 >= 255)
	{
		sum1 -= 255;
	}

	uint16_t sum2 = (acc >> 8) + sum1;
	if (sum2 >= 255)
	{
		sum2 -= 255;
	}

	return (sum2 << 8) | sum1;
}

static inline __attribute__((unused)) uint16_t checksum_final(uint16_t acc) { return acc; }

static inline __attribute__((unused)) bool checksum_verify8(const uint8_t *data, uint8_t size)
{
	if (size < 2)
	{
		// Not enough size for checksum
		return false;
	}

	uint16_t checksum = checksum_reset();
	for (uint8_t i = 0; i < size - 2; i++)
	{
		checksum = checksum_update(checksum, data[i]);
	}

	uint16_t stored = ((uint16_t) (data[size - 1]) << 8) | data[size - 2];
	return checksum_final(checksum) == stored;
}

static inline __attribute__((unused)) bool checksum_verify(const uint8_t *data, size_t size)
{
	if (size < 2)
	{
		// Not enough size for checksum
		return false;
	}

	uint16_t checksum = checksum_reset();
	for (size_t i = 0; i < size - 2; i++)
	{
		checksum = checksum_update(checksum, data[i]);
	}

	uint16_t stored = ((uint16_t) (data[size - 1]) << 8) | data[size - 2];
	return checksum_final(checksum) == stored;
}
