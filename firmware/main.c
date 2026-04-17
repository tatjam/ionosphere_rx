// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dac_spi.h"
#include <avr/io.h>
#include <util/delay.h>

int main()
{
	CLKPR = 0b10000000;
	CLKPR = 0b00000000;
	dac_spi_init();

	DDRA |= (1 << PA1);
	PORTA |= (1 << PA1);

	uint16_t value = 0;
	while (1)
	{
		dac_spi_send(value << 2);
		value += 1;

		if (value >= 1024)
		{
			value = 0;
			PORTA ^= (1 << PA1);
		}

		// _delay_ms(3);
	}

	return 0;
}
