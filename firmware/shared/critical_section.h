// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#ifndef ENV_TEST
#include <util/atomic.h>
#define CRITICAL_SECTION ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
#else
#define CRITICAL_SECTION
#endif
