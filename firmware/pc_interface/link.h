// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "command.h"
#include <stdbool.h>

#define LINK_FSK_HIGH 300000
#define LINK_FSK_LOW  200000

// - Uses Timer2, in particular OC2B, so pin 3 in Arduino Nano (PD3) for the FSK signal
void link_init();

void link_task();

void link_handle_command(const command_t *cmd);

bool link_wants_enter_programming();
