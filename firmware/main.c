// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dac_spi.h"
#include <util/delay.h>

int main()
{
	dac_spi_init();

	uint16_t value = 0;
	while (1)
	{
		dac_spi_send(value);
		value += 1;

		if (value >= 4095)
		{
			value = 0;
		}

		_delay_ms(3);
	}

	return 0;
}
