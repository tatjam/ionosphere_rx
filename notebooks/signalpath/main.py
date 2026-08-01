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

plt.style.use("seaborn-v0_8-whitegrid")

ts, samples, digital = source.generate_source_signal()
dt = ts[1] - ts[0]

if config.LOOP_FILTER_IS_CHARGE_PUMP:
    netlist = config.NETLIST_LOOP_FILTER_CURRENT
else:
    netlist = config.NETLIST_LOOP_FILTER_VOLTAGE

cct = lcapy.Circuit(netlist)

if config.LOOP_FILTER_IS_CHARGE_PUMP:
    tf = cct.impedance("P1").subs(config.VALUES_LOOP_FILTER_CURRENT)
else:
    tf = cct.transfer("P1", "P2").subs(config.VALUES_LOOP_FILTER_VOLTAGE)

filter = tf.dlti_filter(method="bilinear").subs({"Delta_t": dt})
filter_a = filter.a.fval
filter_b = filter.b.fval

vco_fslope = (config.VCO_F_VMAX - config.VCO_F_VMIN) / (
    config.VCO_VMAX - config.VCO_VMIN
)

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


def phase_comp_xor(voltage, ref=0.0, vco=0.0):
    while True:
        out = voltage
        if (ref > 0.5 and vco > 0.5) or (ref < 0.5 and vco < 0.5):
            out = 0.0

        ref, vco = yield out


def phase_comp_flipflop(voltage, ref=0.0, vco=0.0):
    """
    PC3 on 74HC4046AD, uses a single SR flipflop, but is a voltage based device
    """
    flipflop_state = 0.0
    while True:
        s = 1.0 if ref > 0.5 and flipflop_state < 0.5 else 0.0
        r = 1.0 if vco > 0.5 and flipflop_state > 0.5 else 0.0

        # Impossible due to flipflop state being opposite in both, in real life
        # small jitter could arise, but in practice atleast one is 0.0
        assert s * r == 0.0

        if r > 0.5:
            flipflop_state = 0.0
        elif s > 0.5:
            flipflop_state = 1.0

        ref, vco = yield flipflop_state * voltage


def phase_comp_pump(current, ref=0.0, vco=0.0):
    """
    PC2 on 74HC4046AD, stateful as it uses D flipflops. Output is current
    being either sourced (1.0), sunk (-1.0) or high impedance state (0.0).
    """
    flipflop1_state = 0
    flipflop2_state = 0

    # clock signal needs to detect edges
    last_vco = 0.0
    last_ref = 0.0

    rising_vco = False
    rising_ref = False

    while True:
        reset_signal = 1.0 if flipflop1_state > 0.5 and flipflop2_state > 0.5 else 0.0

        if reset_signal > 0.5:
            flipflop1_state = 0.0
            flipflop2_state = 0.0
        else:
            if rising_vco:
                flipflop2_state = 1.0
            if rising_ref:
                flipflop1_state = 1.0

        up = flipflop1_state > 0.5
        down = flipflop2_state > 0.5

        out = 0.0
        if up and down:
            out = 0.0
        elif up:
            out = current
        elif down:
            out = -current

        last_vco = vco
        last_ref = ref

        ref, vco = yield out

        rising_vco = last_vco < 0.5 and vco > 0.5
        rising_ref = last_ref < 0.5 and ref > 0.5


# Pre-initialized with zeros so history exists
filter_vin = [0] * len(filter_a)
filter_vout = [0] * len(filter_a)
vco = []
ref = []

vco_phase = 0.0


if config.PHASE_COMP == "PC1":
    phase_comp = phase_comp_xor(config.PC_VOLTAGE)
elif config.PHASE_COMP == "PC2":
    assert config.LOOP_FILTER_IS_CHARGE_PUMP
    phase_comp = phase_comp_pump(config.PC_CURRENT)
elif config.PHASE_COMP == "PC3":
    phase_comp = phase_comp_flipflop(config.PC_VOLTAGE)
else:
    assert False, "Invalid PHASE_COMP value"

next(phase_comp)

for i in range(0, len(ts)):
    ref_sample = samples[i]
    vco_val = 0.5 * (np.sign(np.sin(vco_phase)) + 1.0)
    # If the VCO is sine-based (rare)
    # vco_val = 0.5 * (np.sin(vco_phase) + 1.0)
    ref.append(ref_sample)
    vco.append(vco_val)
    comp = phase_comp.send((ref_sample, vco_val))
    filter_vin.append(comp)
    filter_vout.append(utils.apply_filter(filter_a, filter_b, filter_vin, filter_vout))

    # Integrate the output of the filter
    vco_input = filter_vout[-1]
    vco_f = min(max(vco_input, config.VCO_VMIN), config.VCO_VMAX)
    vco_f = config.VCO_F_VMIN + (vco_f - config.VCO_VMIN) * vco_fslope

    vco_phase += 2 * np.pi * vco_f * dt

filter_vin = filter_vin[len(filter_a) :]
filter_vout = filter_vout[len(filter_a) :]


# sbn.lineplot(x=ts, y=filter_vin, label="filter V_in (V)")
sbn.lineplot(x=ts, y=filter_vout, label="filter V_out (V)")
sbn.lineplot(x=ts, y=digital, label="Digital signal (V)")
# sbn.lineplot(x=ts, y=vco, label="VCO (V)")
# sbn.lineplot(x=ts, y=ref, label="Ref (V)")

plt.xlabel("t (s)")
plt.show()

# fft_signal = np.array(vco)
# plt.figure()
# utils.plot_fft(fft_signal, config.SIGNAL_FS, ts[0], ts[-1], 0, 500e3)
# plt.show()
