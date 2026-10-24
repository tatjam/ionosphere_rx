// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later

// Bits are 01010101 so maximally optimal for clock sync
// NOTE: This is because the line is idle-high (the PLL is idle-low, but we have an
// inverting comparator afterwards!). The TX side inverts the sent bits so they are
// correctly reflected, thus the sync word has to be correct for the real, after
// comparator, idle line level.
// NOTE: If the after-comparator line becomes idle-low, change to 0xAA
#define MANCHESTER_SYNC_WORD 0x55
