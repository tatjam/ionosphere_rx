// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later

#include "link.h"
#include "programmer.h"
#include "timebase.h"
#include "uart.h"

#include <avr/builtins.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdbool.h>

#include <util/delay.h>

#define PROGRAMMING_MODE_MAGIC 0xEA
static uint8_t EEMEM saved_programming_mode;

void software_reset()
{
	wdt_enable(WDTO_1S);
	while(1) {

	}
}

int programming_main() {
	programmer_init();

	while(1)
	{
		bool continue_programming = programmer_task();
		if(!continue_programming) {
			eeprom_update_byte(&saved_programming_mode, 0);
			software_reset();
		}
	}

	return 0;
}

int link_main() {
	timebase_init();

	link_init();
	while (1)
	{
		link_task();
		if (link_wants_enter_programming())
		{
			eeprom_update_byte(&saved_programming_mode, PROGRAMMING_MODE_MAGIC);
			software_reset();
		}
	}

	return 0;
}

// 100% sure to clear the watchdog before main, ideally the arduino bootloader does
// it already!
void wdt_early(void) __attribute__((naked, used, section(".init3")));
void wdt_early(void)
{
	__asm__ __volatile__(
		"clr r1\n\t"
		"out %[mcusr], r1\n\t"           // MCUSR = 0
		"ldi r24, %[unlock]\n\t"
		"sts %[wdtcsr], r24\n\t"         // WDCE | WDE: start the timed sequence
		"sts %[wdtcsr], r1\n\t"          // WDT off (within 4 cycles)
		:
		: [mcusr]  "I" (_SFR_IO_ADDR(MCUSR)),
		  [wdtcsr] "n" (_SFR_MEM_ADDR(WDTCSR)),
		  [unlock] "M" (_BV(WDCE) | _BV(WDE))
		: "r24");
}

int main()
{
	// Set clock pre-scaler to 1 for max speed operation
	cli();
	CLKPR = 0b10000000;
	CLKPR = 0b00000000;

	bool programming_mode          = false;
	uint8_t programming_mode_magic = eeprom_read_byte(&saved_programming_mode);
	if (programming_mode_magic == PROGRAMMING_MODE_MAGIC)
	{
		programming_mode = true;
	}

	uart_init();
	sei();

	if (programming_mode)
	{
		return programming_main();
	}
	else
	{
		return link_main();
	}

	return 0;
}
