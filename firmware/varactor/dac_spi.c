// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dac_spi.h"
#include <util/delay.h>

void dac_spi_init()
{
	DAC_SPI_DDR_CS |= (1 << DAC_SPI_PIN_CS);
	DAC_SPI_DDR_SCK |= (1 << DAC_SPI_PIN_SCK);
	DAC_SPI_DDR_SDI |= (1 << DAC_SPI_PIN_SDI);

	// Pull ~{CS} high
	DAC_SPI_PORT_CS |= 1 << DAC_SPI_PIN_CS;

	// Clock down
	DAC_SPI_PORT_SCK &= ~(1 << DAC_SPI_PIN_SCK);
}

static inline void clock_bit(uint8_t on)
{
	if (on)
	{
		DAC_SPI_PORT_SDI |= 1 << DAC_SPI_PIN_SDI;
	}
	else
	{
		DAC_SPI_PORT_SDI &= ~(1 << DAC_SPI_PIN_SDI);
	}

	DAC_SPI_PORT_SCK |= 1 << DAC_SPI_PIN_SCK;
	DAC_SPI_PORT_SCK &= ~(1 << DAC_SPI_PIN_SCK);
}

void dac_spi_send(uint16_t value)
{
	// ~{CS} down to initiate transaction
	DAC_SPI_PORT_CS &= ~(1 << DAC_SPI_PIN_CS);

	// Write to DAC A
	clock_bit(0);
	// Use unbuffered Vref
	clock_bit(0);
	// Gain = 1, as otherwise precision is lost near upper edge
	clock_bit(1);
	// Output enabled
	clock_bit(1);

	// Clock the data bits, we do the trick to make each loop iteration same
	// time, instead of doing the bitshift >> i
	uint16_t mask = 0x800;
	for (uint8_t i = 0; i < 12; i++)
	{
		clock_bit((value & mask) ? 1 : 0);
		mask >>= 1;
	}

	// CS up, at this point the data is updated
	DAC_SPI_PORT_CS |= 1 << DAC_SPI_PIN_CS;
}
