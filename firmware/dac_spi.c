// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dac_spi.h"

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

static void clock_bit(uint8_t on)
{
	if (on)
	{
		DAC_SPI_PORT_SDI |= 1 << DAC_SPI_PIN_SDI;
	}
	else
	{
		DAC_SPI_PORT_SDI &= ~(1 << DAC_SPI_PIN_SDI);
	}

	// Clock the SCK port, up then down. At max freq of the chip, 20MHz, the clock is still at 10MHz
	// within DAC limits
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
	// Gain = 2, for full output swing
	clock_bit(0);
	// Output enabled
	clock_bit(1);

	// Clock the data bits
	for (uint8_t i = 0; i < 12; i++)
	{
		clock_bit(value >> (12 - i) & 1);
	}

	// CS up, at this point the data is updated
	DAC_SPI_PORT_CS |= 1 << DAC_SPI_PIN_CS;
}
