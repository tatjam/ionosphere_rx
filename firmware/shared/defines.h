// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later

// Bits are 10101010 so maximally optimal for clock sync
// If we change to a idle-high line, 0x55 is needed so it starts
// with a HIGH->LOW transition
#define MANCHESTER_SYNC_WORD 0xAA
