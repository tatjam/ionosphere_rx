// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later

// Peripherals:
// - Uses Timer0 for internal timing purposes
// - Uses Timer2, in particular OC2B, so pin 3 in Arduino Nano (PD3) for the FSK signal

#pragma once

#include "command.h"
#include <stdbool.h>

#define LINK_FSK_HIGH    300000
#define LINK_FSK_LOW     200000
#define SEQUENCE_RB_SIZE 256

#define LINK_GOOD_BYTE 'G'
#define LINK_BAD_BYTE  'B'

typedef enum
{
	LINK_NO_ERROR,
	LINK_LINK_SLOW,
	LINK_SYMBOLS_OVERFLOW,
	LINK_SYMBOLS_STOPPED,
	LINK_SYMBOLS_STARTED,
	LINK_SYMBOLS_START_IN_PAST,
} link_error_t;

_Static_assert(SEQUENCE_RB_SIZE > MAX_COMMAND_SYMBOLS,
               "The symbol queue must atleast fit a single command");

void link_init();

void link_handle_command(const command_t *cmd);

bool link_wants_enter_programming();
