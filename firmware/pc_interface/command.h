// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
	COMMAND_NO_OP,
	COMMAND_ENTER_PROGRAMMER,
	COMMAND_SEND_SEQUENCE,
	COMMAND_SET_CLOCK,
	COMMAND_GET_CLOCK,
	COMMAND_LAST,
} command_kind_t;

typedef struct
{
	uint16_t symbols[100];
	uint8_t num_symbols;
	uint32_t starting_second;
	uint16_t ticks_between_symbols;
} command_send_payload_t;

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
		command_send_payload_t command_send_payload;
		command_set_clock_t command_set_clock;
	};
} command_t;

bool command_decode(uint8_t *data, uint8_t size, command_t *out);
