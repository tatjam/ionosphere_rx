// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later
// Adapted from the ArduinoISP program
#include "programmer.h"
#include "uart.h"
#include <stddef.h>
#include <util/delay.h>

#ifndef ENV_TEST
#include <avr/io.h>

static void programmer_hw_acquire_pins(bool reset_active_high)
{
	TARGET_MOSI_DDR |= _BV(TARGET_MOSI_PIN);
	TARGET_SCK_DDR |= _BV(TARGET_SCK_PIN);
	TARGET_MISO_DDR &= ~_BV(TARGET_MISO_PIN);

	// Pull reset to inactive state to not shutdown the device
	if (reset_active_high)
	{
		TARGET_RESET_PORT &= ~_BV(TARGET_RESET_PIN);
	}
	else
	{
		TARGET_RESET_PORT |= _BV(TARGET_RESET_PIN);
	}
	TARGET_RESET_DDR |= _BV(TARGET_RESET_PIN);
}

static void programmer_hw_release_pins()
{
	TARGET_MOSI_DDR &= ~_BV(TARGET_MOSI_PIN);
	TARGET_SCK_DDR &= ~_BV(TARGET_SCK_PIN);
	TARGET_MISO_DDR &= ~_BV(TARGET_MISO_PIN);
	// NOTE: This technically leaves the pin weakly pulled-up if it's reset-on-low,
	// but that's okay
	TARGET_RESET_DDR &= ~_BV(TARGET_RESET_PIN);
}

// Transmit a byte, receive a byte
static uint8_t programmer_hw_spi_transmit(uint8_t b)
{
	for (uint8_t i = 0; i < 8; i++)
	{
		// Send MSB
		if (b & 0x80)
		{
			TARGET_MOSI_PORT |= _BV(TARGET_MOSI_PIN);
		}
		else
		{
			TARGET_MOSI_PORT &= ~_BV(TARGET_MOSI_PIN);
		}

		// Clock up
		TARGET_SCK_PORT |= _BV(TARGET_SCK_PIN);
		_delay_us(5);

		// Read LSB from target into b itself after shifting up
		b <<= 1;
		if ((TARGET_MISO_PINREG & TARGET_MISO_PIN) != 0)
		{
			b |= 1;
		}

		// Clock down
		TARGET_SCK_PORT &= ~_BV(TARGET_SCK_PIN);
		_delay_us(5);
	}

	return b;
}

#endif

static uint8_t wait_for_byte()
{
	while (uart_get_next_count() == 0)
	{
	}

	return uart_read_next();
}

#define STK_OK      0x10
#define STK_FAILED  0x11
#define STK_UNKNOWN 0x12
#define STK_INSYNC  0x14
#define STK_NOSYNC  0x15
#define CRC_EOP     0x20

static void reply(const char *str)
{
	if (wait_for_byte() == CRC_EOP)
	{
		uart_tx(STK_INSYNC);
		if (str != NULL)
		{
			uart_tx_cstr(str);
		}
		uart_tx(STK_OK);
	}
	else
	{
		uart_tx(STK_NOSYNC);
	}
}

static void breply(const uint8_t c)
{
	if (wait_for_byte() == CRC_EOP)
	{
		uart_tx(STK_INSYNC);
		uart_tx(c);
		uart_tx(STK_OK);
	}
	else
	{
		uart_tx(STK_NOSYNC);
	}
}

static uint8_t buf[256];
// Params
static uint8_t param_devicecode;
static uint8_t param_revision;
static uint8_t param_progtype;
static uint8_t param_parmode;
static uint8_t param_polling;
static uint8_t param_selftimed;
static uint8_t param_lockbytes;
static uint8_t param_fusebytes;
static uint8_t param_flashpoll;
static uint16_t param_eeprompoll;
static uint16_t param_pagesize;
static uint16_t param_eepromsize;
static uint32_t param_flashsize;

static bool rst_active_high;
static bool pmode;

static uint32_t here;

static void fill(uint16_t num)
{
	for (uint16_t x = 0; x < num; x++)
	{
		buf[x] = wait_for_byte();
	}
}

static void get_version()
{
	uint8_t sub = wait_for_byte();
	if (sub == 0x80)
	{
		// HWVER
		breply(2);
	}
	else if (sub == 0x81)
	{
		// SWMAJ
		breply(1);
	}
	else if (sub == 0x82)
	{
		// SWMIN
		breply(18);
	}
	else if (sub == 0x93)
	{
		// Kind
		breply('S');
	}
	else
	{
		breply(0);
	}
}

// The protocol is big-endian:
#define beget16(addr) (*addr * 256 + *(addr + 1))

static void set_parameters()
{
	fill(20);

	param_devicecode = buf[0];
	param_revision   = buf[1];
	param_progtype   = buf[2];
	param_parmode    = buf[3];
	param_polling    = buf[4];
	param_selftimed  = buf[5];
	param_lockbytes  = buf[6];
	param_fusebytes  = buf[7];
	param_flashpoll  = buf[8];
	// buf[9] unused
	param_eeprompoll = beget16(&buf[10]);
	param_pagesize   = beget16(&buf[12]);
	param_eepromsize = beget16(&buf[14]);
	param_flashsize  = (uint32_t) buf[16] << 24;
	param_flashsize |= (uint32_t) buf[17] << 16;
	param_flashsize |= (uint32_t) buf[18] << 8;
	param_flashsize |= (uint32_t) buf[19];

	rst_active_high = param_devicecode >= 0xe0;
}

static void reset_target(bool reset)
{
	if ((reset && rst_active_high) || (!reset && !rst_active_high))
	{
		TARGET_RESET_PORT |= _BV(TARGET_RESET_PIN);
	}
	else
	{
		TARGET_RESET_PORT &= ~_BV(TARGET_RESET_PIN);
	}
}

static void start_pmode()
{
	programmer_hw_acquire_pins(rst_active_high);
	reset_target(true);

	// Reset under clock down
	TARGET_SCK_PORT &= ~_BV(TARGET_SCK_PIN);
	_delay_ms(20);
	reset_target(false);
	_delay_us(100);
	reset_target(true);

	_delay_ms(50);
	programmer_hw_spi_transmit(0xAC);
	programmer_hw_spi_transmit(0x53);
	programmer_hw_spi_transmit(0x00);
	programmer_hw_spi_transmit(0x00);
	pmode = true;
}

static void end_pmode()
{
	reset_target(false);
	pmode = false;
	programmer_hw_release_pins();
}

static void read_signature()
{
	if (wait_for_byte() != CRC_EOP)
	{
		uart_tx(STK_NOSYNC);
		return;
	}
	uart_tx(STK_INSYNC);
	programmer_hw_spi_transmit(0x30);
	programmer_hw_spi_transmit(0x00);
	programmer_hw_spi_transmit(0x00);
	uint8_t high = programmer_hw_spi_transmit(0x00);
	uart_tx(high);
	programmer_hw_spi_transmit(0x30);
	programmer_hw_spi_transmit(0x00);
	programmer_hw_spi_transmit(0x01);
	uint8_t middle = programmer_hw_spi_transmit(0x00);
	uart_tx(middle);
	programmer_hw_spi_transmit(0x30);
	programmer_hw_spi_transmit(0x00);
	programmer_hw_spi_transmit(0x02);
	uint8_t low = programmer_hw_spi_transmit(0x00);
	uart_tx(low);
	uart_tx(STK_OK);
}

uint16_t current_page()
{
	if (param_pagesize == 32)
	{
		return here & 0xFFFFFFF0;
	}
	if (param_pagesize == 64)
	{
		return here & 0xFFFFFFE0;
	}
	if (param_pagesize == 128)
	{
		return here & 0xFFFFFFC0;
	}
	if (param_pagesize == 256)
	{
		return here & 0xFFFFFF80;
	}
	return here;
}

static void commit(unsigned int addr)
{
	programmer_hw_spi_transmit(0x4C);
	programmer_hw_spi_transmit((addr >> 8) & 0xFF);
	programmer_hw_spi_transmit(addr & 0xFF);
	programmer_hw_spi_transmit(0);
	_delay_ms(30);
}

static void flash(bool high, uint16_t addr, uint8_t data)
{
	if (high)
	{
		programmer_hw_spi_transmit(0x48);
	}
	else
	{
		programmer_hw_spi_transmit(0x40);
	}
	programmer_hw_spi_transmit(addr >> 8 & 0xFF);
	programmer_hw_spi_transmit(addr & 0xFF);
	programmer_hw_spi_transmit(data);
}

static uint8_t write_flash_pages(uint16_t len)
{
	uint16_t x    = 0;
	uint16_t page = current_page();
	while (x < len)
	{
		if (page != current_page())
		{
			commit(page);
			page = current_page();
		}

		flash(false, here, buf[x++]);
		flash(true, here, buf[x++]);
		here++;
	}

	commit(page);

	return STK_OK;
}

static void write_flash(uint16_t len)
{
	fill(len);
	if (wait_for_byte() == CRC_EOP)
	{
		uart_tx(STK_INSYNC);
		uart_tx(write_flash_pages(len));
	}
	else
	{
		uart_tx(STK_NOSYNC);
	}
}

static uint8_t write_eeprom_chunk(uint16_t start, uint16_t len)
{
	fill(len);
	for (uint16_t x = 0; x < len; x++)
	{
		unsigned int addr = start + x;
		programmer_hw_spi_transmit(0xC0);
		programmer_hw_spi_transmit((addr >> 8) & 0xFF);
		programmer_hw_spi_transmit(addr & 0xFF);
		programmer_hw_spi_transmit(buf[x]);
		_delay_ms(45);
	}
	return STK_OK;
}

#define EECHUNK (32)
uint8_t write_eeprom(uint16_t length)
{
	// here is a word address, get the byte address
	uint16_t start     = here * 2;
	uint16_t remaining = length;
	if (length > param_eepromsize)
	{
		return STK_FAILED;
	}
	while (remaining > EECHUNK)
	{
		write_eeprom_chunk(start, EECHUNK);
		start += EECHUNK;
		remaining -= EECHUNK;
	}
	write_eeprom_chunk(start, remaining);
	return STK_OK;
}

static void universal()
{
	fill(4);
	programmer_hw_spi_transmit(buf[0]);
	programmer_hw_spi_transmit(buf[1]);
	programmer_hw_spi_transmit(buf[2]);
	uint8_t ch = programmer_hw_spi_transmit(buf[3]);
	breply(ch);
}

static void program_page()
{
	uint8_t result = STK_FAILED;
	uint32_t len   = (uint32_t) wait_for_byte() << 8;
	len |= wait_for_byte();

	uint8_t memtype = wait_for_byte();
	if (memtype == 'F')
	{
		write_flash(len);
		return;
	}
	else if (memtype == 'E')
	{
		result = write_eeprom(len);
		if (wait_for_byte() == CRC_EOP)
		{
			uart_tx(STK_INSYNC);
			uart_tx(result);
		}
		else
		{
			uart_tx(STK_NOSYNC);
		}
		return;
	}

	uart_tx(STK_FAILED);
}

static uint8_t flash_read(bool high, unsigned int addr)
{
	if (high)
	{
		programmer_hw_spi_transmit(0x28);
	}
	else
	{
		programmer_hw_spi_transmit(0x20);
	}
	programmer_hw_spi_transmit((addr >> 8) & 0xFF);
	programmer_hw_spi_transmit(addr & 0xFF);
	return programmer_hw_spi_transmit(0);
}

static uint8_t flash_read_page(uint16_t len)
{
	for (uint16_t x = 0; x < len; x += 2)
	{
		uint8_t low = flash_read(false, here);
		uart_tx(low);
		uint8_t high = flash_read(true, here);
		uart_tx(high);
		here++;
	}
	return STK_OK;
}

uint8_t eeprom_read_page(uint16_t len)
{
	uint16_t start = here * 2;
	for (uint16_t x = 0; x < len; x++)
	{
		int addr = start + x;
		programmer_hw_spi_transmit(0xA0);
		programmer_hw_spi_transmit((addr >> 8) & 0xFF);
		programmer_hw_spi_transmit(addr & 0xFF);
		uint8_t ee = programmer_hw_spi_transmit(0xFF);
		uart_tx(ee);
	}
	return STK_OK;
}

void read_page()
{
	uint8_t result  = STK_FAILED;
	uint16_t length = (uint16_t) wait_for_byte() << 8;
	length |= wait_for_byte();
	uint8_t memtype = wait_for_byte();

	if (wait_for_byte() != CRC_EOP)
	{
		uart_tx(STK_NOSYNC);
		return;
	}
	uart_tx(STK_INSYNC);
	if (memtype == 'F')
	{
		result = flash_read_page(length);
	}
	if (memtype == 'E')
	{
		result = eeprom_read_page(length);
	}
	uart_tx(result);
}

void programmer_init()
{
	here            = 0;
	rst_active_high = false;
	programmer_hw_release_pins();
}

// Return true if we need to continue in programming mode
bool programmer_task()
{
	uint8_t byte = wait_for_byte();

	if (byte == '0')
	{
		// Sync sequence
		reply(NULL);
	}
	else if (byte == '1')
	{
		// Identify
		reply("AVR ISP");
	}
	else if (byte == 'A')
	{
		get_version();
	}
	else if (byte == 'B')
	{
		set_parameters();
		reply(NULL);
	}
	else if (byte == 'E')
	{
		// Extra parameters, unimplemeneted
		fill(5);
		reply(NULL);
	}
	else if (byte == 'P')
	{
		if (!pmode)
		{
			start_pmode();
		}
		reply(NULL);
	}
	else if (byte == 'U')
	{
		here = (uint32_t) wait_for_byte();
		here |= (uint32_t) wait_for_byte() << 8;
		reply(NULL);
	}
	else if (byte == 0x60)
	{
		// Unimplemented?
		wait_for_byte();
		wait_for_byte();
		reply(NULL);
	}
	else if (byte == 0x61)
	{
		// Unimplemented?
		wait_for_byte();
		reply(NULL);
	}
	else if (byte == 0x64)
	{
		program_page();
	}
	else if (byte == 0x74)
	{
		read_page();
	}
	else if (byte == 'V')
	{
		universal();
	}
	else if (byte == 'Q')
	{
		end_pmode();
		reply(NULL);
		return false;
	}
	else if (byte == 0x75)
	{
		read_signature();
	}
	else if (byte == CRC_EOP)
	{
		// Restore sync
		uart_tx(STK_NOSYNC);
	}
	else
	{
		uint8_t next = wait_for_byte();
		if (next == CRC_EOP)
		{
			uart_tx(STK_UNKNOWN);
		}
		else
		{
			uart_tx(STK_NOSYNC);
		}
	}

	return true;
}
