# Copyright (C) 2026 tatjam
# SPDX-License-Identifier: GPL-3.0-or-later
from scipy.signal import lfilter

import numpy as np
from scipy import signal
from scipy.signal import spectrogram
import matplotlib.pyplot as plt
import seaborn as sbn
import lcapy

# Sim Config
fs = 10e6
duration = 7e-3
f_ref = 130e3
f_step = 90e3
t_step = 3.5e-3

fft_intervals = [
    (2e-3, 3.5e-3),
    (5e-3, 7.0e-3),
]

vco_vmin = 0.0
vco_vmax = 1.0
vco_fvmin = 10e3
vco_fslope = 200e3

is_charge_pump = False

# netlist_charge_pump = """
# I_in 	in	0   ac
# R1		in  mid
# C1      mid	0
# R2      mid out
# C2      out 0
# """

# values_charge_pump = {
# 	"R1": 10e3,
# 	"C1": 10e-9,
# 	"R2": 1e3,
# 	"C2": 1e-9
# }

netlist_voltage = """
P1      1	0
R1		1	2
C1		2	2a
R2      2a  0
P2      2   0
"""

values_voltage = {"R1": 10e3, "C1": 1e-8, "R2": 1e3}

#######################################
dt = 1.0 / fs
t = np.arange(0, duration, dt)

f_in = np.where(t < t_step, f_ref, f_step)
# phi = integral(2 pi f) dt, so in a sum it's just (extracting factors)
phase_in = 2 * np.pi * np.cumsum(f_in) * dt
v_in = np.sin(phase_in)


if is_charge_pump:
    netlist = netlist_charge_pump
else:
    netlist = netlist_voltage

cct = lcapy.Circuit(netlist)

if is_charge_pump:
    # TODO, impedance transfer function
    pass
else:
    tf = cct.transfer("P1", "P2").subs(values_voltage)

filter = tf.dlti_filter(method="bilinear").subs({"Delta_t": dt})
filter_a = filter.a.fval
filter_b = filter.b.fval

# Statistics we can pre-compute now...

# XOR
k_d = 1.0 / np.pi
k_vco = 2 * np.pi * vco_fslope
h_0 = tf.subs({"s": 0}).cval
delta_f_lock = k_d * k_vco * h_0.real / (2 * np.pi)


print(f"Two-sided lock range: {delta_f_lock/1e3:.1f} kHz")
f_center = vco_fvmin + 0.5 * (vco_vmax - vco_vmin) * vco_fslope
print(
    f"Lock range around f_center: {(f_center - delta_f_lock/2)/1e3:.1f} - {(f_center + delta_f_lock/2)/1e3:.1f} kHz"
)

tf.bode_plot((0.01 * vco_fvmin, 100.0 * (vco_fvmin + vco_fslope * (vco_vmax - vco_vmin))))
plt.show()


def apply_filter(filter_a, filter_b, vin, vout):
    """
    State contains the last samples, indexed as
            vin[n] = vin[-1], vin[n - 1] = vin[-2] and
            vout[n - 1] = vout[-1],
    vout[n] is the returned value.
    """
    assert len(filter_a) == len(filter_b)

    # V_out(z) / V_in(z) = (b[0] + b[1] z^(-1) + ...) / (a[0] + a[1] z^(-1) + ...)
    # in time domain
    # a0 V_out[n] + a1 V_out[n - 1] + ... = b0 V_in[n] + b1 V_in[n - 1] + ...
    # V_out[n] = 1 / a0 ( b0 V_in[n] + b1 V_in[n - 1] + ... - a1 V_out[n - 1] - ...)
    out = filter_b[0] * vin[-1]
    for i in range(1, len(filter_a)):
        # Note: vout[-1] is vout[n-1], but vin needs one more in the past as it's known by now!
        out += filter_b[i] * vin[-(i + 1)] - filter_a[i] * vout[-i]

    out /= filter_a[0]

    return out


def phase_comp_xor(ref, vco):
    return 1.0 if ref != vco else 0.0


# Pre-initialized with zeros so history exists
filter_vin = [0] * len(filter_a)
filter_vout = [0] * len(filter_a)
vco = []
ref = []

vco_phase = 0.0

for i in range(0, len(t)):
    ref_sample = 0.5 * (np.sign(v_in[i]) + 1.0)
    vco_val = 0.5 * (np.sign(np.sin(vco_phase)) + 1.0)
    ref.append(ref_sample)
    vco.append(vco_val)
    comp = phase_comp_xor(ref_sample, vco_val)
    filter_vin.append(comp)
    filter_vout.append(apply_filter(filter_a, filter_b, filter_vin, filter_vout))

    # Integrate the output of the filter
    vco_input = filter_vout[-1]
    vco_f = 0.0
    if vco_vmin < vco_input and vco_input < vco_vmax:
        vco_f = vco_fvmin + (vco_input - vco_vmin) * vco_fslope

    vco_phase += 2 * np.pi * vco_f * dt

filter_vin = filter_vin[len(filter_a) :]
filter_vout = filter_vout[len(filter_a) :]


sbn.lineplot(x=t, y=filter_vin, label="filter V_in (V)")
sbn.lineplot(x=t, y=filter_vout, label="filter V_out (V)")
# sbn.lineplot(x=t, y=vco, label="VCO (V)")
# sbn.lineplot(x=t, y=ref, label="Ref (V)")

plt.xlabel("t (s)")
plt.show()

fft_signal = np.array(vco)

for i, (t_start, t_end) in enumerate(fft_intervals):
    idx_start = int(t_start * fs)
    idx_end = int(t_end * fs)
    segment = fft_signal[idx_start:idx_end]

    n = len(segment)
    if n == 0:
        print(f"Interval {i} ({t_start}s - {t_end}s) is empty, skipping")
        continue

    # Window to reduce spectral leakage
    window = np.hanning(n)
    segment_windowed = segment * window

    spectrum = np.fft.rfft(segment_windowed)
    freqs = np.fft.rfftfreq(n, d=dt)
    mag_db = 20 * np.log10(np.abs(spectrum) + 1e-20)

    plt.figure()
    sbn.lineplot(x=freqs, y=mag_db)
    plt.xlabel("Frequency (Hz)")
    plt.ylabel("Magnitude (dB)")
    plt.title(f"FFT: t = {t_start*1e3:.2f}ms to {t_end*1e3:.2f}ms")
    plt.xlim(0, 500e3)  # zoom near f_ref/f_step and low-order spurs; adjust as needed
    plt.show()
