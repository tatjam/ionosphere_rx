// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later
#include "command.h"
#include "../shared/checksum.h"

static bool parse_set_clock_command(uint8_t *payload, uint8_t size, command_set_clock_t *out)
{

	// [gps_second (4 bytes)] [nudge (2 bytes)]
	if (size < 6)
	{
		return false;
	}

	out->second = (uint32_t) payload[0];
	out->second |= (uint32_t) payload[1] << 8;
	out->second |= (uint32_t) payload[2] << 16;
	out->second |= (uint32_t) payload[3] << 24;

	out->clock_nudge = (uint16_t) payload[4];
	out->clock_nudge |= (uint16_t) payload[5] << 8;

	return true;
}

static bool parse_start_sequence_command(uint8_t *payload, uint8_t size,
                                         command_start_sequence_t *out)
{
	// [num symbols (1 byte)] [starting second (4 bytes)] [ticks btw. symbols (2 bytes)]
	// [symbols]
	if (size < 7)
	{
		return false;
	}

	out->num_symbols = payload[0];

	if (out->num_symbols > MAX_COMMAND_SYMBOLS)
	{
		return false;
	}

	uint8_t symbol_payload = size - 7;

	if (symbol_payload < out->num_symbols * 2)
	{
		return false;
	}

	out->starting_second = (uint32_t) payload[1];
	out->starting_second |= (uint32_t) payload[2] << 8;
	out->starting_second |= (uint32_t) payload[3] << 16;
	out->starting_second |= (uint32_t) payload[4] << 24;

	out->ticks_between_symbols = (uint16_t) payload[5];
	out->ticks_between_symbols |= (uint16_t) payload[6] << 8;

	for (uint8_t symbol = 0; symbol < out->num_symbols; symbol++)
	{
		out->symbols[symbol] = (uint16_t) payload[7 + symbol * 2 + 0];
		out->symbols[symbol] |= (uint16_t) payload[7 + symbol * 2 + 1] << 8;
	}

	return true;
}

static bool parse_append_sequence_command(uint8_t *payload, uint8_t size,
                                          command_append_sequence_t *out)
{
	// [num symbols (1 byte)] [symbols]
	if (size < 1)
	{
		return false;
	}

	out->num_symbols = payload[0];

	if (out->num_symbols > MAX_COMMAND_SYMBOLS)
	{
		return false;
	}

	uint8_t symbol_payload = size - 1;

	if (symbol_payload < out->num_symbols * 2)
	{
		return false;
	}

	for (uint8_t symbol = 0; symbol < out->num_symbols; symbol++)
	{
		out->symbols[symbol] = (uint16_t) payload[1 + symbol * 2 + 0];
		out->symbols[symbol] |= (uint16_t) payload[1 + symbol * 2 + 1] << 8;
	}

	return true;
}

bool command_decode(uint8_t *data, uint8_t size, command_t *out)
{
	// Packets are [command type] [per-command data xN] [checksum byte 0] [checksum byte 1]
	if (size < 3)
	{
		return false;
	}

	uint8_t command_payload = size - 3;

	if (!checksum_verify8(data, size))
	{
		return false;
	}

	out->kind = data[0];

	if (out->kind >= COMMAND_LAST)
	{
		return false;
	}

	if (out->kind == COMMAND_NO_OP || out->kind == COMMAND_ENTER_PROGRAMMER ||
	    out->kind == COMMAND_GET_CLOCK || out->kind == COMMAND_QUERY_SYMBOL_QUEUE ||
	    out->kind == COMMAND_GET_ERROR)
	{
		return true;
	}
	else if (out->kind == COMMAND_SET_CLOCK)
	{
		return parse_set_clock_command(data + 1, command_payload, &out->command_set_clock);
	}
	else if (out->kind == COMMAND_START_SEQUENCE)
	{
		return parse_start_sequence_command(data + 1, command_payload,
		                                    &out->command_start_sequence);
	}
	else if (out->kind == COMMAND_APPEND_SEQUENCE)
	{
		return parse_append_sequence_command(data + 1, command_payload,
		                                     &out->command_append_sequence);
	}

	return true;
}
