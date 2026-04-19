// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dac_spi.h"
#include <avr/io.h>
#include <avr/interrupt.h>
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
		if (value >= 3440)
		{
			dac_spi_send(3440);
			value = 820;
			PORTA ^= (1 << PA1);
		}
		else {
			dac_spi_send(value);
			value += 1;
		}

		_delay_us(1000);
	}

	return 0;
}
