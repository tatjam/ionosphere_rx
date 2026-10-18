// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "command.h"
#include <stdbool.h>

#define LINK_FSK_HIGH    300000
#define LINK_FSK_LOW     200000
#define SEQUENCE_RB_SIZE 256

#define LINK_GOOD_BYTE 'G'
#define LINK_BAD_BYTE  'B'

_Static_assert(SEQUENCE_RB_SIZE > MAX_COMMAND_SYMBOLS,
               "The symbol queue must atleast fit a single command");

// - Uses Timer2, in particular OC2B, so pin 3 in Arduino Nano (PD3) for the FSK signal
void link_init();

void link_task();

void link_handle_command(const command_t *cmd);

bool link_wants_enter_programming();
