import itertools
import numpy as np

TUNING_TABLE = {
    0: 2.17e-10,
    32: 2.17e-10,
    64: 2.17e-10,
    96: 2.17e-10,
    128: 2.18e-10,
    160: 2.18e-10,
    192: 2.18e-10,
    224: 2.10e-10,
    256: 2.03e-10,
    320: 1.93e-10,
    384: 1.83e-10,
    448: 1.75e-10,
    512: 1.69e-10,
    576: 1.63e-10,
    640: 1.58e-10,
    704: 1.53e-10,
    768: 1.49e-10,
    832: 1.45e-10,
    896: 1.41e-10,
    960: 1.38e-10,
    1024: 1.35e-10,
    1280: 1.20e-10,
    1536: 1.10e-10,
    1792: 1.00e-10,
    2048: 9.1e-11,
    2304: 8.4e-11,
    2560: 7.8e-11,
    2816: 7.3e-11,
    3072: 6.8e-11,
    3328: 6.5e-11,
    3584: 6.1e-11,
    3840: 5.9e-11,
    4095: 5.6e-11,
}

OFF = (False, False, False)
CHECK_CODES = [256, 1024, 2048, 4092]
RELAY_OFFSETS_PF = {
    (False, False, False): [0.0, 0.0, 0.0, 0.0],
    (False, False, True): [428.0, 441.0, 448.0, 455.0],
    (False, True, False): [208.0, 216.0, 221.0, 227.0],
    (False, True, True): [616.0, 629.0, 638.0, 646.0],
    (True, False, False): [96.0, 102.0, 105.0, 106.0],
    (True, False, True): [514.0, 527.0, 536.0, 543.0],
    (True, True, False): [296.0, 308.0, 314.0, 319.0],
    (True, True, True): [699.0, 714.0, 723.0, 731.0],
}


def build_model(tuning_table, check_codes, relay_offsets_pf, margin=4e-12):
    """Usage:
    solve = build_model(TUNING_TABLE, CHECK_CODES, RELAY_OFFSETS_PF)
    relays, code = solve(500e-12, current_relays=prev_relays)
    """
    pts = sorted((int(c), v) for c, v in tuning_table.items())
    t_codes = np.array([c for c, _ in pts])
    t_caps = np.array([v for _, v in pts])

    # Inverse table: keep only the last point of the flat plateau at the start
    keep = np.r_[t_caps[1:] < t_caps[:-1], True]
    inv_codes, inv_caps = t_codes[keep], t_caps[keep]
    c_max, c_min = inv_caps[0], inv_caps[-1]

    # Per relay combination: C_total = A * C_tune + B  (fitted in pF)
    x_pf = np.interp(check_codes, t_codes, t_caps) * 1e12
    fits = {OFF: (1.0, 0.0)}
    for relays, offs in relay_offsets_pf.items():
        if relays == OFF:
            continue
        y_pf = x_pf + np.array(offs)
        A, B = np.polyfit(x_pf, y_pf, 1)
        res = y_pf - (A * x_pf + B)
        worst = int(np.argmax(np.abs(res)))
        if len(x_pf) > 3 and abs(res[worst]) > 5.0:  # drop one obvious outlier, refit
            print(
                f"{[int(r) for r in relays]}: dropping outlier at code "
                f"{check_codes[worst]} ({res[worst]:+.1f} pF)"
            )
            ok = np.arange(len(x_pf)) != worst
            A, B = np.polyfit(x_pf[ok], y_pf[ok], 1)
        fits[relays] = (A, B * 1e-12)

    def cap_to_code(c):
        code = np.interp(c, inv_caps[::-1], inv_codes[::-1])  # needs ascending x
        return int(round(code / 4) * 4)  # 10-bit DAC grid

    def candidates(target, m):
        out = []
        for r, (A, B) in fits.items():
            ct = (target - B) / A  # tuning capacitance needed with r on
            if c_min + m <= ct <= c_max - m:
                out.append((r, ct))
        return out

    def solve(target, current_relays=None):
        cands = candidates(target, margin) or candidates(target, 0.0)
        if not cands:
            lo = min(A * c_min + B for A, B in fits.values())
            hi = max(A * c_max + B for A, B in fits.values())
            raise ValueError(
                f"{target * 1e12:.1f} pF is out of range "
                f"({lo * 1e12:.0f} to {hi * 1e12:.0f} pF)"
            )

        cur = tuple(current_relays) if current_relays is not None else None
        for r, ct in cands:
            if r == cur:  # hysteresis: keep current relays
                return list(r), cap_to_code(ct)

        # Lowest C_tune = highest code = flattest part of the curve = finest steps
        r, ct = min(cands, key=lambda rc: rc[1])
        return list(r), cap_to_code(ct)

    return solve
