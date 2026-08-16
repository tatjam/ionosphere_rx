# Copyright (C) 2026 tatjam
# SPDX-License-Identifier: GPL-3.0-or-later

from scipy import signal

# SOURCE SIGNAL
#######################

F0 = 160e3
F1 = 310e3
SIGNAL_TO_ENCODE = [ord(x) for x in "\x01\x02\x03\x04\x05"]

# Clock rate of the MANCHESTER encoded signal, so SIGNAL_TO_ENCODE is transmitted at half this
SIGNAL_CLOCK_RATE = 24e3
# How many samples per bit of MANCHESTER encoded signal
SIGNAL_OVERSAMPLING = 50e2

SIGNAL_FS = SIGNAL_CLOCK_RATE * SIGNAL_OVERSAMPLING

# "F0", "F1" or None
SOURCE_STEADY_TONE = None
SOURCE_USE_FILTER = True
SOURCE_FILTER_PASS = 500e3
SOURCE_FILTER_ORDER = 3
SOURCE_FILTER = signal.butter(
    SOURCE_FILTER_ORDER, SOURCE_FILTER_PASS, btype="lowpass", fs=SIGNAL_FS
)

PHASE_CONTINUITY = True


# RECEPTION PLL
#######################

VCO_VMIN = 0.06
VCO_VMAX = 0.94
VCO_F_VMIN = 150e3
VCO_F_VMAX = 350e3


# Phase comparator output voltage
PC_VOLTAGE = 1.0
# Phase comparator output current (sink or drain)
PC_CURRENT = 500e-6

LOOP_FILTER_IS_CHARGE_PUMP = True

NETLIST_LOOP_FILTER_VOLTAGE = """
P1      1	0
R1		1	2
C1		2	2a
R2      2a  0
P2      2   0
"""

VALUES_LOOP_FILTER_VOLTAGE = {"R1": 1e3, "C1": 1e-8, "R2": 1e2}


# NETLIST_LOOP_FILTER_CURRENT = """
# P1      1	0
# R1		1	2
# C1		2	0
# C2		1	0
# """

# VALUES_LOOP_FILTER_CURRENT = {"R1": 10e3, "C1": 100e-9, "C2": 1e-9}

NETLIST_LOOP_FILTER_CURRENT = """
P1      1	0
C2		1	0
"""

VALUES_LOOP_FILTER_CURRENT = {"C2": 1e-9}

# "PC1", "PC2" (requires charge pump) or "PC3"
PHASE_COMP = "PC2"


OUTPUT_FILTER_PASS = 48e3
OUTPUT_FILTER_ORDER = 2
OUTPUT_FILTER = signal.butter(
    OUTPUT_FILTER_ORDER, OUTPUT_FILTER_PASS, btype="lowpass", fs=SIGNAL_FS
)
