# Copyright (C) 2026 tatjam
# SPDX-License-Identifier: GPL-3.0-or-later
import math

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

    def gen_vco(self, vtune_node: str, vout_node: str):
        lines = []
        # VCO tuning from vtune_node, output to vout_node

        # V -> I converter
        i_node = f"{self.internal_name}ia"

        # G (linear voltage-controlled-current source),
        # controlled by the voltage at the tnode, and outputs into a
        # capacitor for integration. 1V -> 1A
        cur = f"max(min(V({vtune_node}), {self.max_v}), {self.min_v})"
        lines.append(f"G{self.internal_name}g {i_node} 0 cur='{cur}'")

        # Integrator capacitor, value of 1F, so i(t) = dV/dt, and the voltage
        # at the i_node is the integral of tnode voltage
        lines.append(f"C{self.internal_name}intc {i_node} 0 1")

        # Phase node, to convert the integral of tnode (phsae) into a voltage
        # into vout_node
        total_cycles = f"({self.f_at_v0} * time + {self.f_slope} * v({i_node}))"
        # total_cycles  = f"({self.f_at_v0} * time)"
        lines.append(f"B{self.internal_name}b {vout_node} 0 v=sin(2*{math.pi} * {total_cycles})")

        return lines

    def get_pco_node(self):
        return f"{self.internal_name}pco"

    def gen_phase_comp_xor(self, in1_node: str, in2_node: str, out_node: str):
        lines = []
        scale = self.max_v

        lines.append(
            f"B{self.internal_name}xor {out_node} 0 "
            f"v='{scale}*0.5*(1 - sgn(V({in1_node})) * sgn(V({in2_node})))'"
        )

        return lines

    def gen_phase_comp_flipflop(self, in1_node: str, in2_node: str, out_node: str):
        lines = []

        return lines

    def gen_filter(self, vin_node: str, vout_node: str):
        lines = []
        # RC network

        # r1: vin_node -> middle node 1 (output)
        tnode = vout_node
        lines.append(f"R{self.internal_name}r1 {vin_node} {tnode} {self.r1}")

        # r2 (optional): middle node 1 (output) -> middle node 2
        if self.r2 is not None:
            prev_tnode = tnode
            tnode = f"{self.internal_name}mn2"
            lines.append(f"R{self.internal_name}r2 {prev_tnode} {tnode} {self.r2}")

        # c: from mn1 or mn2 to gnd
        lines.append(f"C{self.internal_name}c {tnode} 0 {self.c}")

        return lines

    def generate_full(self, in_node: str, pll_out_node: str, filtered_out_node: str, use_xor: bool):
        lines = []

        inbuf_node = f"{self.internal_name}ibufnode"
        pco_node = self.get_pco_node()
        vco_out_node = f"{self.internal_name}vcoo"
        filter_node = f"{self.internal_name}filter"

        # buffer for input
        lines.append(f"B{self.internal_name}ibuf {inbuf_node} 0 v='v({in_node})'")

        # VCO
        lines += self.gen_phase_comp_xor(inbuf_node, vco_out_node, pco_node)
        lines += self.gen_filter(pco_node, filter_node)
        lines += self.gen_vco(filter_node, vco_out_node)

        # buffer for output (filtered)
        lines.append(f"B{self.internal_name}obufpll {filtered_out_node} 0 v='v({filter_node})'")
        # buffer for output (pll)
        lines.append(f"B{self.internal_name}obuf {pll_out_node} 0 v='v({vco_out_node})'")


        return lines
