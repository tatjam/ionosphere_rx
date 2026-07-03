# Copyright (C) 2026 tatjam
# SPDX-License-Identifier: GPL-3.0-or-later

class Pll:
    def __init__(
        self,
        internal_name,
        f_at_v0,
        f_slope,
        min_v,
        max_v,
        r1,
        r2,
        c,
    ):
        self.internal_name = internal_name
        self.f_at_v0 = f_at_v0
        self.f_slope = f_slope
        self.min_v = min_v
        self.max_v = max_v
        self.r1 = r1
        self.r2 = r2
        self.c = c

    def gen_spice(self, vin_node: str, vout_node: str):
        lines = []
        # RC network

        # r1: phase comparator -> middle node 1
        pco_node = f"{self.internal_name}pco"
        tnode = f"{self.internal_name}mn1"
        lines.append(f"R{self.internal_name}r1 {pco_node} {tnode} {self.r1}")

        # r2 (optional): middle node -> middle node 2
        if self.r2 is not None:
            prev_tnode = tnode
            tnode = f"{self.internal_name}mn2"
            lines.append(f"R{self.internal_name}r2 {prev_tnode} {tnode} {self.r2}")

        # c: from mn1 or mn2 to gnd
        lines.append(f"C{self.internal_name}c {tnode} 0 {self.c}")
        vco_exp = "1"
        lines.append(f"B{self.internal_name}b {pco_node} 0 v={vco_exp}")

        # VCO tuning from tnode, output to pco_node
        return lines
