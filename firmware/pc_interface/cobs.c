// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later
#include "cobs.h"
#include <assert.h>

void cobs_init(cobs_decoder_t *c, uint8_t *buffer, uint8_t size)
{
	c->buffer       = buffer;
	c->size         = size;
	c->ptr          = 0;
	c->num_to_zero  = 0;
	c->state        = LINK_ALIGN;
	c->packet_start = true;
}

bool cobs_feed(cobs_decoder_t *c, uint8_t byte, uint8_t *len)
{
	if (c->ptr >= c->size)
	{
		// Buffer exhausted mid-packet, drop it and resynchronize
		c->state       = LINK_ALIGN;
		c->ptr         = 0;
		c->num_to_zero = 0;
	}

	if (c->state == LINK_ALIGN)
	{
		if (byte != 0)
		{
			return false;
		}

		// Found the COBS delimiter, start reading bytes!
		c->state        = LINK_START;
		c->ptr          = 0;
		c->packet_start = true;
		return false;
	}

	if (c->state == LINK_COLLECT)
	{
		assert(c->num_to_zero > 0);
		c->num_to_zero--;
		if (c->num_to_zero != 0)
		{
			// Chunk data byte
			c->buffer[c->ptr++] = byte;
			return false;
		}

		// Chunk completed, this byte is the next code byte
	}

	// Read the COBS number of bytes to next zero
	if (byte == 0)
	{
		if (c->state == LINK_START)
		{
			// Empty packet, skip
			return false;
		}

		// Packet completed
		*len            = c->ptr;
		c->ptr          = 0;
		c->packet_start = true;
		c->state        = LINK_START;
		return true;
	}

	if (byte >= c->size)
	{
		// Impossible code byte for this buffer size, drop the packet
		c->state       = LINK_ALIGN;
		c->ptr         = 0;
		c->num_to_zero = 0;
		return false;
	}

	if (!c->packet_start)
	{
		// Insert a zero
		c->buffer[c->ptr++] = 0;
	}
	c->packet_start = false;
	c->num_to_zero  = byte;
	c->state        = LINK_COLLECT;
	return false;
}
