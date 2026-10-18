// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later
#include "command.h"
#include "../shared/checksum.h"

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
	    out->kind == COMMAND_GET_CLOCK)
	{
		return out;
	}
	else if (out->kind == COMMAND_SET_CLOCK)
	{
		// [gps_second (4 bytes)] [nudge (2 bytes)]
		if (command_payload < 6)
		{
			return false;
		}

		out->command_set_clock.second = (uint32_t) data[1];
		out->command_set_clock.second |= (uint32_t) data[2] << 8;
		out->command_set_clock.second |= (uint32_t) data[3] << 16;
		out->command_set_clock.second |= (uint32_t) data[4] << 24;

		out->command_set_clock.clock_nudge = (uint16_t) data[5];
		out->command_set_clock.clock_nudge |= (uint16_t) data[6] << 8;
	}
	else if (out->kind == COMMAND_SEND_SEQUENCE)
	{
		// [num symbols (1 byte)] [starting second (4 bytes)] [ticks btw. symbols (2 bytes)]
		// [symbols]
		if (command_payload < 7)
		{
			return false;
		}

		out->command_send_payload.num_symbols = data[1];

		uint8_t symbol_payload = command_payload - 7;

		if (symbol_payload < out->command_send_payload.num_symbols * 2)
		{
			return false;
		}

		out->command_send_payload.starting_second = (uint32_t) data[2];
		out->command_send_payload.starting_second |= (uint32_t) data[3] << 8;
		out->command_send_payload.starting_second |= (uint32_t) data[4] << 16;
		out->command_send_payload.starting_second |= (uint32_t) data[5] << 24;

		out->command_send_payload.ticks_between_symbols = (uint16_t) data[6];
		out->command_send_payload.ticks_between_symbols |= (uint16_t) data[7] << 8;

		for (uint8_t symbol = 0; symbol < out->command_send_payload.num_symbols; symbol++)
		{
			out->command_send_payload.symbols[symbol] = (uint16_t) data[8 + symbol * 2 + 0];
			out->command_send_payload.symbols[symbol] |= (uint16_t) data[8 + symbol * 2 + 1] << 8;
		}
	}

	return true;
}
