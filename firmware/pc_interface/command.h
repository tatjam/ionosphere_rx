// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
	COMMAND_NO_OP,
	COMMAND_ENTER_PROGRAMMER,
} command_kind_t;

typedef struct
{
	command_kind_t kind;
} command_t;

bool command_decode(uint8_t *data, uint8_t size, command_t *out);
