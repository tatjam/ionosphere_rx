// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later

#include "link.h"
#include "programmer.h"
#include "timebase.h"
#include "uart.h"

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

	bool programming_mode = false;

	uart_init();
	timebase_init();
	programmer_init();

	link_init();

	sei();

	while (1)
	{
		if (programming_mode)
		{
			programming_mode = programmer_task();
		}
		else
		{
			link_task();
			if (link_wants_enter_programming())
			{
				programming_mode = true;
			}
		}
	}

	return 0;
}
