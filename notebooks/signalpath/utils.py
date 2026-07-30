# Copyright (C) 2026 tatjam
# SPDX-License-Identifier: GPL-3.0-or-later
import numpy as np
from numpy.typing import NDArray
import matplotlib.pyplot as plt
from scipy import signal

import utils


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


def apply_filter_full(filter_a, filter_b, vin: NDArray) -> NDArray:
    """
    Same as apply_filter, but does it over the entire signal, for open-loop systems,
    obviously way faster!
    """
    return signal.lfilter(filter_b, filter_a, vin)


def plot_fft(fft_signal, fs, t_start, t_end, x0, x1, is_power=False):
    dt = 1.0 / fs
    idx_start = int(t_start * fs)
    idx_end = int(t_end * fs)
    segment = fft_signal[idx_start:idx_end]

    n = len(segment)
    if n == 0:
        print(f"Interval ({t_start}s - {t_end}s) is empty, skipping")
        return

    window = np.hanning(n)
    segment_windowed = segment * window

    spectrum = np.fft.rfft(segment_windowed)
    freqs = np.fft.rfftfreq(n, d=dt)

    mult = 10 if is_power else 20

    mag = np.abs(spectrum) + 1e-20
    mag_db = mult * np.log10(mag)

    valid_indices = np.where((freqs >= x0) & (freqs <= x1))
    if len(valid_indices[0]) > 0:
        peak_val = np.max(mag[valid_indices])
    else:
        peak_val = np.max(mag)

    mag_db -= mult * np.log10(peak_val)

    plt.semilogx(freqs, mag_db)
    plt.xlabel("Frequency (Hz)")
    plt.ylabel("Magnitude (dB rel to peak)")
    plt.title(f"FFT: t = {t_start*1e3:.2f}ms to {t_end*1e3:.2f}ms")
    plt.xlim(x0, x1)
    plt.ylim(-150, 0)
