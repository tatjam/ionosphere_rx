# Copyright (C) 2026 tatjam
# SPDX-License-Identifier: GPL-3.0-or-later
from scipy.signal import lfilter

import numpy as np
from scipy import signal
from scipy.signal import spectrogram
import matplotlib.pyplot as plt
import seaborn as sbn
import lcapy

import utils
import source
import config

ts, samples = source.generate_source_signal()
dt = ts[1] - ts[0]

if config.LOOP_FILTER_IS_CHARGE_PUMP:
	# TODO
	assert False
else:
    netlist = config.NETLIST_LOOP_FILTER_VOLTAGE

cct = lcapy.Circuit(netlist)

if config.LOOP_FILTER_IS_CHARGE_PUMP:
	# TODO
	assert False
else:
    tf = cct.transfer("P1", "P2").subs(config.VALUES_LOOP_FILTER_VOLTAGE)

filter = tf.dlti_filter(method="bilinear").subs({"Delta_t": dt})
filter_a = filter.a.fval
filter_b = filter.b.fval

vco_fslope = (config.VCO_F_VMAX - config.VCO_F_VMIN) / (config.VCO_VMAX - config.VCO_VMIN)

# XOR
k_d = 1.0 / np.pi
k_vco = 2 * np.pi * vco_fslope
h_0 = tf.subs({"s": 0}).cval
delta_f_lock = k_d * k_vco * h_0.real / (2 * np.pi)


print(f"Two-sided lock range: {delta_f_lock/1e3:.1f} kHz")
f_center = (config.VCO_F_VMIN + config.VCO_F_VMAX) / 2
print(
    f"Lock range around f_center: {(f_center - delta_f_lock/2)/1e3:.1f} - {(f_center + delta_f_lock/2)/1e3:.1f} kHz"
)

def phase_comp_xor(ref, vco):
    if (ref > 0.5 and vco > 0.5) or (ref < 0.5 and vco < 0.5):
        return 0.0
    else:
        return 1.0


# Pre-initialized with zeros so history exists
filter_vin = [0] * len(filter_a)
filter_vout = [0] * len(filter_a)
vco = []
ref = []

vco_phase = 0.0

for i in range(0, len(ts)):
    ref_sample = 0.5 * (np.sign(samples[i]) + 1.0)
    vco_val = 0.5 * (np.sign(np.sin(vco_phase)) + 1.0)
    # If the VCO is sine-based (rare)
    # vco_val = 0.5 * (np.sin(vco_phase) + 1.0)
    ref.append(ref_sample)
    vco.append(vco_val)
    comp = phase_comp_xor(ref_sample, vco_val)
    filter_vin.append(comp)
    filter_vout.append(utils.apply_filter(filter_a, filter_b, filter_vin, filter_vout))

    # Integrate the output of the filter
    vco_input = filter_vout[-1]
    vco_f = 0.0
    if config.VCO_VMIN < vco_input and vco_input < config.VCO_VMAX:
        vco_f = config.VCO_F_VMIN + (vco_input - config.VCO_VMIN) * vco_fslope

    vco_phase += 2 * np.pi * vco_f * dt

filter_vin = filter_vin[len(filter_a) :]
filter_vout = filter_vout[len(filter_a) :]


sbn.lineplot(x=ts, y=filter_vin, label="filter V_in (V)")
sbn.lineplot(x=ts, y=filter_vout, label="filter V_out (V)")
# sbn.lineplot(x=t, y=vco, label="VCO (V)")
# sbn.lineplot(x=t, y=ref, label="Ref (V)")

plt.xlabel("t (s)")
plt.show()

fft_signal = np.array(vco)
plt.figure()
utils.plot_fft(fft_signal, config.SIGNAL_FS, ts[0], ts[-1], 0, 500e3)
plt.show()
