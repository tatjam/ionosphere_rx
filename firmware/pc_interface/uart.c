// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later

#include "uart.h"
#include "../shared/ring_buffer.h"
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sfr_defs.h>

// Large enough as to hold the largest avrisp command with ample margin, so we
// never overflow!
#define UART_BUFFER_BYTES 256
RING_BUFFER_DEFINE(uart_rb, UART_BUFFER_BYTES, uint8_t, uint8_t);
static uart_rb_t rb;

ISR(USART_RX_vect)
{
	if (UCSR0A & _BV(FE0))
	{
		return;
	}

	uart_rb_put(&rb, UDR0);
}

#define UBRR_VALUE (F_CPU / 16 / UART_BAUD - 1)

void uart_init()
{
	uart_rb_init(&rb);

	// Baud
	UBRR0H = (uint8_t) (UBRR_VALUE >> 8);
	UBRR0L = (uint8_t) UBRR_VALUE;

	// Enable RX and TX
	UCSR0B |= _BV(RXEN0) | _BV(TXEN0);
	// 8 bit of data
	UCSR0C |= _BV(UCSZ01) | _BV(UCSZ00);
	// Enable receive interrupt
	UCSR0B |= _BV(RXCIE0);
}

void uart_tx(uint8_t data)
{
	loop_until_bit_is_set(UCSR0A, UDRE0);
	UDR0 = data;
}

void uart_tx_cstr(const char *str)
{
	uint8_t ptr = 0;
	while (str[ptr] != 0)
	{
		uart_tx((uint8_t) str[ptr]);
	}
}

uint8_t uart_get_next_count() { return uart_rb_count(&rb); }

uint8_t uart_read_next() { return uart_rb_get(&rb); }
