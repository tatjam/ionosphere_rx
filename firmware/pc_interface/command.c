// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later
#include "command.h"
#include "../shared/checksum.h"

bool command_decode(uint8_t *data, uint8_t size, command_t *out)
{
	if (!checksum_verify8(data, size))
	{
		return false;
	}

	out->kind = COMMAND_NO_OP;

	return true;
}
