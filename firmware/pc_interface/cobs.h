// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
	LINK_ALIGN,
	LINK_START,
	LINK_COLLECT,
} link_task_state_t;

typedef struct
{
	link_task_state_t state;
	uint8_t *buffer;
	uint8_t size;
	uint8_t ptr;
	uint8_t num_to_zero;
	// True right after a delimiter or a completed packet, i.e. the next chunk
	// is the first one of its packet and no zero is inserted before it
	bool packet_start;
} cobs_decoder_t;

void cobs_init(cobs_decoder_t *c, uint8_t *buffer, uint8_t size);

// Feeds one wire byte. Returns true when a complete packet was decoded into
// c->buffer, with its length in *len. Returns false otherwise (delimiters,
// garbage, aborted and empty packets are silently dropped)
bool cobs_feed(cobs_decoder_t *c, uint8_t byte, uint8_t *len);
