// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later
// (Taken from https://jera.com/techinfo/jtns/jtn002)

#pragma once
#define ENV_TEST

#include <stdio.h>

#define mu_assert(message, test)                                                                   \
	do                                                                                             \
	{                                                                                              \
		if (!(test))                                                                               \
			return message;                                                                        \
	} while (0)
#define mu_run_test(test)                                                                          \
	do                                                                                             \
	{                                                                                              \
		char *message = test();                                                                    \
		tests_run++;                                                                               \
		if (message)                                                                               \
			return message;                                                                        \
	} while (0)

#define mu_suite(result) \
	do { \
     if (result != 0) { \
         printf("%s\n", result); \
     } \
     else { \
         printf("ALL TESTS PASSED\n"); \
     } \
     printf("Tests run: %d\n", tests_run); \
     return result != 0; \
} while (0)


extern int tests_run;
