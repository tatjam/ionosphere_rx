# Copyright (C) 2026 tatjam
# SPDX-License-Identifier: GPL-3.0-or-later
from numpy.typing import NDArray

import seaborn as sbn
import matplotlib.pyplot as plt
import numpy as np

import config
import utils


def signal_to_bits(signal: list[int]) -> list[bool]:
    out = []
    for i in range(len(signal)):
        bits = f"{signal[i]:08b}"
        for bit in bits:
            out.append(bit == "1")

    return out


def manchester_encode(
    signal: list[bool], preamble: list[bool] | None = None
) -> list[bool]:
    """
    Give signal as an array of bytes, generates the manchester encoded bistream
    that represents it. Adds the optional preamble if it's not None.
    """
    bits: list[bool] = []

    clock = False
    for i in range(0, len(signal) * 2):
        # Note != is XOR for booleans
        bits.append(signal[i // 2] != clock)
        clock = not clock

    if preamble is not None:
        return preamble + bits
    else:
        return bits


def fsk_modulate(
    bits: list[bool],
    bitrate,
    bitrate_mult: int,
    f0,
    f1,
    phase_continuity=True,
    digital=True,
) -> tuple[NDArray, NDArray, NDArray]:
    """
    Generates BFSK modulated from the bitstream at given bitrate, generating the samples
    at the given multiple of bitrate. Ensure bitrate_mult*bitrate is > 2*max(f0, f1).

    Returns the ts, samples, digital sample pair
    """

    # Time goes from 0 to len(bits) / bitrate, and we have bitrate_mult samples per bit, so...
    ts = np.linspace(0, len(bits) / bitrate, len(bits) * bitrate_mult)
    dt = ts[1] - ts[0]

    samples = np.zeros(len(ts))
    digital_out = np.zeros(len(ts))

    # TODO: we could add a low pass filter to the digital signal to smooth the harmonics

    phase = 0.0
    for i in range(0, len(bits)):
        for samp in range(0, bitrate_mult):
            samples[i * bitrate_mult + samp] = np.sin(phase)
            f = f1 if bits[i] else f0
            phase += 2.0 * np.pi * f * dt

            digital_out[i * bitrate_mult + samp] = 1.0 if bits[i] else 0.0

        if not phase_continuity:
            phase = 0.0

    if digital:
        samples = (samples >= 0).astype(float)

    return ts, samples, digital_out


def generate_source_signal():
    bits = manchester_encode(signal_to_bits(config.SIGNAL_TO_ENCODE))
    if config.SOURCE_STEADY_TONE is not None:
        if config.SOURCE_STEADY_TONE == "F0":
            bits = [False] * len(bits)
        elif config.SOURCE_STEADY_TONE == "F1":
            bits = [True] * len(bits)

    ts, samples, digital_out = fsk_modulate(
        bits,
        config.SIGNAL_CLOCK_RATE,
        int(config.SIGNAL_OVERSAMPLING),
        config.F0,
        config.F1,
        config.PHASE_CONTINUITY,
        True,
    )

    if config.SOURCE_USE_FILTER:
        b = config.SOURCE_FILTER[0]
        a = config.SOURCE_FILTER[1]
        samples = utils.apply_filter_full(a, b, samples)

    return ts, samples, digital_out


if __name__ == "__main__":
    ts, samples, digital = generate_source_signal()
    plt.style.use("seaborn-v0_8-whitegrid")

    plt.figure()
    utils.plot_fft(samples, config.SIGNAL_FS, ts[0], ts[-1], 10e3, 15e6)
    plt.show()

    plt.figure()
    utils.plot_fft(digital - 0.5, config.SIGNAL_FS, ts[0], ts[-1], 0, 100e3)
    plt.show()
