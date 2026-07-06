# Copyright (C) 2026 tatjam
# SPDX-License-Identifier: GPL-3.0-or-later

import os
import sys
# import plotext as plt
import matplotlib.pyplot as plt
from spicelib import SimRunner, SpiceEditor, RawRead
from spicelib.simulators.ngspice_simulator import NGspiceSimulator
from scipy.signal import spectrogram
from scipy.interpolate import interp1d
import numpy as np

sys.path.append(os.path.abspath(os.path.dirname(__file__)))
from pll import Pll

circuit_lines = [".title CoupledPll"]
pll1 = Pll("pll1", 1e6, 1.0e3, 0.3, 5.0, 1.6e3, None, 10e-9)
circuit_lines += pll1.generate_full("vinpll1", "voutpll1", "filterpll1", True)
circuit_lines += [f"Vin vinpll1 0 SIN(0 1 1.200e6)"]
circuit_lines.append(".end")

with open("coupled_pll.cir", "w") as f:
    f.write("\n".join(circuit_lines) + "\n")

print("\n".join(circuit_lines))

runner = SimRunner(simulator=NGspiceSimulator, output_folder="./simulate")
editor = SpiceEditor("coupled_pll.cir")

stop = 1e-3
save = 1e-5
editor.add_instruction(".tran 1e-8 1e-3")
# editor.add_instruction(f".tran 1e-8 {stop + save} {stop}")

task = runner.run(editor)
runner.wait_completion()

assert task and task.raw_file
analysis = RawRead(task.raw_file, dialect="ngspice")

time = analysis.get_trace("time").get_wave()
in_v = analysis.get_trace("V(vinpll1)").get_wave()
vco_v = analysis.get_trace("V(voutpll1)").get_wave()
pll_v = analysis.get_trace("V(filterpll1)").get_wave()
pco_v = analysis.get_trace(f"V({pll1.get_pco_node()})")

plt.plot(time, in_v, label="VCO input")
# plt.plot(time, pco_v, label="PCO")
plt.plot(time, vco_v, label="VCO output")
plt.plot(time, pll_v, label="Filtered")
# plt.plot(time, in_v * vco_v, label="Product")
plt.title("PLL outputs")
plt.xlabel("time (s)")
plt.ylabel("voltage (V)")
plt.legend()
plt.show()


fs = 100 * pll1.f_at_v0  # e.g. 20x oversampling for clean spectrogram resolution
uniform_time = np.arange(time[0], time[-1], 1/fs)
interp_fn = interp1d(time, vco_v, kind='linear')
uniform_signal = interp_fn(uniform_time)

f, t_spec, Sxx = spectrogram(
    uniform_signal,
    fs=fs,
    nperseg=256,       # window size -- tune for time/frequency resolution tradeoff
    noverlap=200,
)

plt.figure(figsize=(10, 5))
plt.pcolormesh(t_spec, f, 10*np.log10(Sxx + 1e-20), shading='auto')
plt.ylabel("Frequency (Hz)")
plt.xlabel("Time (s)")
plt.colorbar(label="Power (dB)")
plt.axhline(pll1.f_at_v0, color='red', linestyle='--', label='f_at_v0')
plt.axhline(pll1.f_at_v0/2, color='orange', linestyle=':', label='f_at_v0/2 (subharmonic)')
plt.legend()
plt.title("VCO output spectrogram")
plt.show()
