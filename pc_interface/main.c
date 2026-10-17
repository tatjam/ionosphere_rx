// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later

#include <avr/builtins.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdbool.h>

#include <util/delay.h>

int main()
{
	// Set clock pre-scaler to 1 for max speed operation
	cli();
	CLKPR = 0b10000000;
	CLKPR = 0b00000000;
	sei();

	while (1)
	{
	}

	return 0;
}

