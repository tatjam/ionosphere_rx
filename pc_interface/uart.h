// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <stdint.h>
#define UART_BAUD 19200

void uart_init();
void uart_tx(uint8_t data);
uint8_t uart_get_next_count();
uint8_t uart_read_next();
