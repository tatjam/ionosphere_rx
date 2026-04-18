// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later

#include "status.h"
#include <avr/io.h>

static uint8_t error_state;

static uint32_t led_counter;

void status_init()
{
	STATUS_DDR_LED |= (1 << STATUS_PIN_LED);
	STATUS_PORT_LED &= ~(1 << STATUS_PIN_LED);
	error_state = 0;
}

static inline void nominal_status_process()
{
	if (led_counter > 0)
	{
		led_counter--;
	}
	else
	{
		STATUS_PORT_LED &= ~(1 << STATUS_PIN_LED);
	}
}

static inline void error_status_process()
{
	if (led_counter > 0)
	{
		led_counter--;
	}
	else
	{
		led_counter = 10000;
	}

	if (led_counter > 5000)
	{
		STATUS_DDR_LED |= (1 << STATUS_PIN_LED);
	}
	else
	{
		STATUS_PORT_LED &= ~(1 << STATUS_PIN_LED);
	}
}

bool status_process()
{
	if (error_state == 0)
	{
		nominal_status_process();
	}
	else
	{
		error_status_process();
	}

	return error_state != 0;
}

void status_blip()
{
	STATUS_DDR_LED |= (1 << STATUS_PIN_LED);
	led_counter = 65536;
}
