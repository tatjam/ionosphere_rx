// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "command.h"
#include <stdbool.h>
#include <stdint.h>

#define LINK_FSK_HIGH 300000
#define LINK_FSK_LOW  200000

// Uses Timer1, in particular OC1A, so pin 9 in Arduino Nano (PB1)
void link_init();

// Return true if we need to enter programming mode
bool link_task();

void link_handle_command(const command_t *cmd);
