// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <stdbool.h>
#include <stdint.h>

// Defines a new ring buffer with the following interface:
// rb_init(rb) -> void: Resets the ring buffer to empty
// rb_empty(rb) -> bool: Returns true if the ring buffer is empty
// rb_count(rb) -> index_t: Returns the number of elements that can be read right now
// rb_put(rb, data_t item) -> bool: Inserts the item and returns true if successful, false if
// overflow rb_commit(rb) -> bool: Assumes the writes has written to rb->buf[write], and advances
// rb_get(rb) -> data_t: Call only if rb_empty() == false, pops the oldest item
// IMPORTANT:
// - Make sure oyu only call rb_get(rb) if rb_empty() == false, otherwise the queue will corrupt!
// - If using index_t >= 8 bits, ensure all calls are protected by ATOMIC_BLOCK
#define RING_BUFFER_DEFINE(type_name, size, index_t, data_t)                                       \
	_Static_assert((size & (size - 1)) == 0, "size must be power of two");                         \
	typedef struct                                                                                 \
	{                                                                                              \
		volatile index_t write;                                                                    \
		volatile index_t read;                                                                     \
		volatile data_t buf[size];                                                                 \
	} type_name##_t;                                                                               \
	static inline void type_name##_init(type_name##_t *r)                                          \
	{                                                                                              \
		r->write = 0;                                                                              \
		r->read  = 0;                                                                              \
	}                                                                                              \
	static inline bool type_name##_empty(const type_name##_t *r) { return r->write == r->read; }   \
	static inline index_t type_name##_count(const type_name##_t *r)                                \
	{                                                                                              \
		return (r->write - r->read) & (size - 1);                                                  \
	}                                                                                              \
	static inline bool type_name##_put(type_name##_t *r, data_t d)                                 \
	{                                                                                              \
		index_t next = (r->write + 1) & (size - 1);                                                \
		if (next == r->read)                                                                       \
		{                                                                                          \
			return false;                                                                          \
		}                                                                                          \
		r->buf[r->write] = d;                                                                      \
		r->write         = next;                                                                   \
		return true;                                                                               \
	}                                                                                              \
	static inline bool type_name##_commit(type_name##_t *r)                                        \
	{                                                                                              \
		index_t next = (r->write + 1) & (size - 1);                                                \
		if (next == r->read)                                                                       \
		{                                                                                          \
			return false;                                                                          \
		}                                                                                          \
		r->write = next;                                                                           \
		return true;                                                                               \
	}                                                                                              \
	static inline data_t type_name##_get(type_name##_t *r)                                         \
	{                                                                                              \
		data_t d = r->buf[r->read];                                                                \
		r->read  = (r->read + 1) & (size - 1);                                                     \
		return d;                                                                                  \
	}
