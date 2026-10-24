// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
	COMMAND_NO_OP,
	COMMAND_ENTER_PROGRAMMER,
	COMMAND_START_SEQUENCE,
	COMMAND_APPEND_SEQUENCE,
	COMMAND_QUERY_SYMBOL_QUEUE,
	COMMAND_SET_CLOCK,
	COMMAND_GET_CLOCK,
	COMMAND_GET_ERROR,
	COMMAND_LAST,
} command_kind_t;

#define MAX_COMMAND_SYMBOLS 100

#define NEXT_SECOND 0xFFFFFFFF

typedef struct
{
	uint16_t symbols[MAX_COMMAND_SYMBOLS];
	uint16_t num_symbols;
	uint32_t starting_second;
	uint16_t ticks_between_symbols;
} command_start_sequence_t;

typedef struct
{
	uint16_t symbols[MAX_COMMAND_SYMBOLS];
	uint16_t num_symbols;
} command_append_sequence_t;

typedef struct
{
	uint32_t second;
	uint16_t clock_nudge;
} command_set_clock_t;

typedef struct
{
	command_kind_t kind;
	union
	{
		command_start_sequence_t command_start_sequence;
		command_append_sequence_t command_append_sequence;
		command_set_clock_t command_set_clock;
	};
} command_t;

_Static_assert(sizeof(command_t) < 250, "command_t is too large");

bool command_decode(uint8_t *data, uint8_t size, command_t *out);
