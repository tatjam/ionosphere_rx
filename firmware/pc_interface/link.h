// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later

#include <avr/io.h>
#include <stdbool.h>

// Uses timer OC1A, so pin 9 in Arduino Nano (PB1)
void link_init();

// Return true if we need to enter programming mode
bool link_task();
