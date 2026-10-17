// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later

#include "uart.h"
#include <avr/io.h>
#include <avr/sfr_defs.h>
#include <avr/interrupt.h>

#define UBRR_VALUE (F_CPU / 16 / UART_BAUD - 1)

volatile static uint8_t rx_buffer_read_pos;
volatile static uint8_t rx_buffer_write_pos;
static uint8_t rx_buffer[64];


ISR(USART_RX_vect)
{

}

void uart_init()
{
	// Baud
	UBRR0H = (uint8_t) (UBRR_VALUE >> 8);
	UBRR0L = (uint8_t) UBRR_VALUE;

	rx_buffer_read_pos = 0;
	rx_buffer_write_pos = 0;

	// Enable RX and TX
	UCSR0B |= _BV(RXEN0) | _BV(TXEN0);
	// 8 bit of data
	UCSR0C |= _BV(UCSZ01) | _BV(UCSZ00);
	// Enable receive interrupt
	UCSR0B |= _BV(RXCIE0);

}

void uart_tx(uint8_t data) {
	loop_until_bit_is_set(UCSR0A, UDRE0);
	UDR0 = data;
}

uint8_t uart_get_next_count() { return 0; }

uint8_t uart_read_next() { return 0; }
