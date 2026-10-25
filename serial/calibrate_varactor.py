from time import sleep
import typing
from typing import Any
import copy
import sys, zmq, json
import itertools


def _read_cap(name: str):
    print(f"Measure for {name}")
    while True:
        val = input("Input capacitance> ")
        mult = None
        if val.endswith("p"):
            mult = 1e-12
        elif val.endswith("n"):
            mult = 1e-9
        elif val.endswith("u"):
            mult = 1e-6
        else:
            print("Invalid unit, try again (input example: 1.5n)")
            continue

        try:
            parsed = float(val[:-1])
        except ValueError:
            print("Unknown input, try again")
            continue

        parsed *= mult
        b = input(f"Parsed value is {parsed}F, is this correct (y/n)?")

        if b == "y":
            return parsed
        else:
            continue


def _retry(s: zmq.Socket[bytes], msg):
    for _ in range(10):
        s.send_json(msg)
        rep = s.recv_json()
        if (
            isinstance(rep, dict)
            and "ok" in rep
            and isinstance(rep["ok"], bool)
            and rep["ok"] == True
        ):
            return

        sleep(1)

    raise RuntimeError("Unable to communicate with device")


# Note: max tuning gives lowest capacitance
MAX_TUNING = 4095
RELAY_TEST_CODE = 4095  # tuning code used when measuring relay offsets
MAX_TUNING = 4095  # DAC discards the 2 LSBs
OFF = (False, False, False)


def _make_msg(msg_orig, relays=OFF, tuning=None):
    """Fresh copy of msg_orig with all three relays set explicitly (and tuning, if given)."""
    msg = typing.cast(Any, copy.deepcopy(msg_orig))
    for i, on in enumerate(relays):
        msg["settings"][0][f"relay{i}"] = bool(on)
    if tuning is not None:
        msg["settings"][0]["tuning"] = tuning
    return msg


def _measure(s, msg, label):
    _retry(s, msg)
    sleep(2)
    _retry(s, msg)
    return _read_cap(label)


def _relay_tuning(s, msg_orig, code=RELAY_TEST_CODE):
    """Relays off, then each relay alone, all at the same tuning code."""
    relay_table = {}
    for i, mask in enumerate((0, 1, 2, 4)):
        relays = tuple(bool(mask & (1 << r)) for r in range(3))
        msg = _make_msg(msg_orig, relays, tuning=code)
        relay_table[mask] = _measure(s, msg, f"Relays {i + 1} / 4 @ {code}")

    offsets = [relay_table[1 << r] - relay_table[0] for r in range(3)]
    print(f"Relays @ code {code}:\n{json.dumps(relay_table)}")
    print(f"Offsets (relay0, relay1, relay2):\n{json.dumps(offsets)}")
    return relay_table


def _tuning_table(s, msg_orig, steps=16):
    """Uniform sweep, relays off."""
    tuning_table = {}
    for i in range(steps):
        tval = round(i / (steps - 1) * MAX_TUNING)
        msg = _make_msg(msg_orig, tuning=tval)
        tuning_table[tval] = _measure(s, msg, f"Tuning {i + 1} / {steps}")

    print(f"Tuning:\n{json.dumps(tuning_table)}")
    return tuning_table


def _tuning_codes():
    codes = list(range(0, 128, 32))
    codes += [128, 160, 192, 224]
    codes += list(range(256, 1025, 64))
    codes += list(range(1280, 4096, 256))
    codes.append(4095)
    return codes


def _nonlinear_tuning_table(s, msg_orig, codes=None):
    """Sweep on a curvature-weighted grid, relays off.
    Pass codes=range(128, 256, 32) to re-measure only part of the range."""
    codes = list(codes) if codes is not None else _tuning_codes()

    tuning_table = {}
    for i, code in enumerate(codes):
        msg = _make_msg(msg_orig, tuning=code)
        tuning_table[code] = _measure(s, msg, f"Tuning {i + 1} / {len(codes)}")

    print(f"Tuning:\n{json.dumps(tuning_table)}")
    return tuning_table


def _relay_check(s, msg_orig, codes=(256, 1024, 2048, 4092)):
    """All 8 relay combinations at several codes; prints offset vs relays-off."""
    combos = list(itertools.product((False, True), repeat=3))
    total = len(combos) * len(codes)

    results = {}
    n = 0
    for relays in combos:
        for code in codes:
            n += 1
            msg = _make_msg(msg_orig, relays, tuning=code)
            label = f"Relays {[int(r) for r in relays]} @ {code} ({n} / {total})"
            results[(relays, code)] = _measure(s, msg, label)

    off = {c: results[(OFF, c)] for c in codes}

    return {r: [(results[(r, c)] - off[c]) * 1e12 for c in codes] for r in combos}


def _glitch_check(
    s, msg_orig, relays=(True, True, False), codes=(1024, 2048, 4092), repeats=3
):
    """Re-measure one relay combination a few times, with relays-off as reference each time."""
    for code in codes:
        offs = []
        for n in range(repeats):
            c_off = _measure(
                s,
                _make_msg(msg_orig, OFF, tuning=code),
                f"off @ {code} ({n + 1}/{repeats})",
            )
            c_on = _measure(
                s,
                _make_msg(msg_orig, relays, tuning=code),
                f"{[int(r) for r in relays]} @ {code} ({n + 1}/{repeats})",
            )
            offs.append((c_on - c_off) * 1e12)
        print(f"code {code}: " + " ".join(f"{o:7.1f}" for o in offs) + " pF")


if __name__ == "__main__":
    msg_orig = {
        "type": "start_varactor",
        "second": "next",
        "ticks": 60025,
        "settings": [
            {"relay0": False, "relay1": False, "relay2": False, "tuning": MAX_TUNING}
        ],
    }

    addr = sys.argv[1]
    s = zmq.Context().socket(zmq.REQ)
    s.setsockopt(zmq.RCVTIMEO, 3000)  # don't hang forever if no reply
    s.connect(addr)

    # _relay_tuning(s, msg_orig)
    # _nonlinear_tuning_table(s, msg_orig)
    # _relay_check(s, msg_orig)
    _glitch_check(s, msg_orig)
