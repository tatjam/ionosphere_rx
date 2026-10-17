// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later
// Adapted from the ArduinoISP program
#include <stdbool.h>
#include <stdint.h>

// NOTE: The implementation is bit-banged so no need for these to match the
// SPI pins!

// Pin 10 on Arduino Nano
#define TARGET_RESET_PIN  PB2
#define TARGET_RESET_PORT PORTB
#define TARGET_RESET_DDR  DDRB

// Pin 11 or MOSI on Arduino Nano
#define TARGET_MOSI_PIN  PB3
#define TARGET_MOSI_PORT PORTB
#define TARGET_MOSI_DDR  DDRB

// Pin 12 or MISO on Arduino Nano
#define TARGET_MISO_PIN  PB4
#define TARGET_MISO_PORT PORTB
#define TARGET_MISO_DDR  DDRB

// Pin 13 or SCK on Arduino Nano
#define TARGET_SCK_PIN  PB5
#define TARGET_SCK_PORT PORTB
#define TARGET_SCK_DDR  DDRB

void programmer_init();

// Return true if we need to continue in programming mode
bool programmer_task();
