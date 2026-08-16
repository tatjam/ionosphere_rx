# Copyright (C) 2026 tatjam
# SPDX-License-Identifier: GPL-3.0-or-later
# Impedance filter for the charge pump, the series resistor between the
# PC output and VCO node is omitted, as its purpose is setting current (assuming
# linearity!)

import lcapy
import matplotlib.pyplot as plt
import numpy as np
from sympy.printing import pprint
import sympy as sp
import plotly.graph_objects as go

# With this filter, we have
# A zero at
#  -1
# ─────
# C₁⋅R₁
# A pole at 0, and another at
#    C₁ + C₂
# - ────────
#   C₁⋅C₂⋅R₁

NETLIST = """
P1      1	0
C2		1	0
"""


# The main idea is, the pole at the origin makes the filter behave at -20dB near DC,
# up to the zero at which point it flattens, and it starts dropping again after the pole.
#
# The filter must keep DC (for the VCO to work correctly) and it must also decently contain
# our baseband signal. Because the baseband is Manchester encoded, we want to keep decent gain
# at clock-rate / 2 to say 2 clock_rate (atleast). At 24kHz clocking, we can place the zero at
# 1kHz, and the pole at 50kHz.
# The absolute value of the resistor must be chosen later once the entire loop is considered.

VALUES = {"C2": 1e-9}

cct = lcapy.Circuit(NETLIST)
tf = cct.impedance("P1")

print("")
pprint(tf.simplify())

print("")
print("Poles:")
pprint(tf.poles().simplify())

print("")
print("Zeros:")
pprint(tf.zeros().simplify())

tf = tf.subs(VALUES)

print("")
pprint(tf)

print("")
print("Poles:")
pprint(tf.poles())

print("")
print("Zeros:")
pprint(tf.zeros())

# tf.bode_plot((1, 1e6))
# plt.show()

VCC = 5

Kp = VCC / (4 * np.pi)  # V/rad

# Assuming lineal VCO
VCO_VMIN = 0.8
VCO_VMAX = VCC - 0.8
VCO_FMIN = 150e3
VCO_FMAX = 350e3
Kvco = 2 * np.pi * (VCO_FMAX - VCO_FMIN) / (VCO_VMAX - VCO_VMIN)  # (rad/s) / V

# Finally we have the series R gain which scales the current output of the phase comparator
# up to lineal analysis. We use this as the tunable gain for root-locus analysis, and it essentially
# is limited by the current capabilities of the PLL chip

s = lcapy.expr("s")
k = lcapy.expr("K")

# TF of the system with the loop cut open, is 1 + G,
# the Kvco / s is the VCO integrator, and the K is the series R that sets the current -> voltage gain.
g = (k * tf * Kvco / s).cancel().sympy

# (the roots of g = 1 are the poles of the closed loop system)
char_eq = sp.together(1 + g)
num, den = sp.fraction(char_eq)

real_parts = []
imag_parts = []
k_values = []
hover = []

for rval in np.geomspace(1e3, 1e6, 100):
    # Current is just VCC / R in our linealized analysis
    kval = VCC / rval
    roots = sp.Poly(num.subs(k.sympy, kval), s.sympy).nroots()

    for root in roots:
        c_root = complex(root)
        real_parts.append(c_root.real)
        imag_parts.append(c_root.imag)
        k_values.append(float(kval))

        hover.append(
            f"<b>K:</b> {kval:.2e}<br>"
            f"<b>R:</b> {rval:.2e}Ohm<br>"
            f"<b>P:</b> {c_root.real:.2e} + {c_root.imag:.2e}j<br>"
            f"<b>Damping:</b> {-c_root.real / abs(c_root):.2e}<br>"
            f"<b>Frequency:</b> {abs(c_root):.2e}<br>"
        )

fig = go.Figure()

fig.add_trace(
    go.Scatter(
        x=real_parts,
        y=imag_parts,
        mode="markers",
        text=hover,
        hoverinfo="text",
    )
)

fig.update_layout(
    xaxis_title="Real",
    yaxis_title="Imaginary",
    xaxis=dict(zeroline=True, zerolinewidth=2, zerolinecolor="black"),
    yaxis=dict(zeroline=True, zerolinewidth=2, zerolinecolor="black"),
    hovermode="closest",
    width=800,
    height=600,
    plot_bgcolor="white",
)

fig.show()
