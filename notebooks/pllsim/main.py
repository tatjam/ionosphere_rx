# Copyright (C) 2026 tatjam
# SPDX-License-Identifier: GPL-3.0-or-later

import os
import sys
import plotext as plt

import PySpice
from PySpice.Spice.Netlist import Circuit

sys.path.append(os.path.abspath(os.path.dirname(__file__)))
from pll import Pll

circuit = Circuit("CoupledPll")
pll1 = Pll("pll1", 1e6, 1.0, 0.3, 5.0, 100, None, 1e-9)
pll1.gen_spice(circuit, "vinpll1", "voutpll1")
print(circuit)

simulator = circuit.simulator(temperature=25, nominal_temperature=25)
analysis = simulator.transient(step_time=1e-8, end_time=1e-6)
