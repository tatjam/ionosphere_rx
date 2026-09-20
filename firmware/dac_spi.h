// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <avr/io.h>
#include <stdint.h>

#define DAC_SPI_PIN_SDI	 PA6
#define DAC_SPI_PORT_SDI PORTA
#define DAC_SPI_DDR_SDI	 DDRA

#define DAC_SPI_PIN_SCK	 PA4
#define DAC_SPI_PORT_SCK PORTA
#define DAC_SPI_DDR_SCK	 DDRA

#define DAC_SPI_PIN_CS	PA5
#define DAC_SPI_PORT_CS PORTA
#define DAC_SPI_DDR_CS	DDRA

void dac_spi_init();
void dac_spi_send(uint16_t value);
