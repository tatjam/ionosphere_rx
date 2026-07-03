# Copyright (C) 2026 tatjam
# SPDX-License-Identifier: GPL-3.0-or-later

import os
import sys
import plotext as plt
from spicelib import SimRunner, SpiceEditor, RawRead
from spicelib.simulators.ngspice_simulator import NGspiceSimulator

sys.path.append(os.path.abspath(os.path.dirname(__file__)))
from pll import Pll

circuit_lines = [".title CoupledPll"]
pll1 = Pll("pll1", 1e6, 1.0, 0.3, 5.0, 100, None, 1e-9)
circuit_lines += pll1.gen_spice("vinpll1", "voutpll1")
circuit_lines.append(".end")

with open("coupled_pll.cir", "w") as f:
    f.write("\n".join(circuit_lines) + "\n")

print("\n".join(circuit_lines))

runner = SimRunner(simulator=NGspiceSimulator, output_folder="./simulate")
editor = SpiceEditor("coupled_pll.cir")
editor.add_instruction(".tran 1e-8 1e-6")

task = runner.run(editor)
runner.wait_completion()

assert task and task.raw_file
analysis = RawRead(task.raw_file, dialect="ngspice")
