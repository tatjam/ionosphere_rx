# Copyright (C) 2026 tatjam
# SPDX-License-Identifier: GPL-3.0-or-later

import numpy as np
import PySpice.Logging.Logging as logging
from PySpice.Spice.Netlist import Circuit
from PySpice.Unit import *


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

    def gen_spice(
        self,
        circuit: Circuit,
        vin_node: str,
        vout_node: str,
    ):
        # RC network

        # r1: phase comparator -> middle node 1
        tnode = f"{self.internal_name}mn1"
        pco_node = f"{self.internal_name}pco"
        circuit.R(
            f"{self.internal_name}r1",
            pco_node,
            tnode,
            self.r1,
        )

        # r2 (optional): middle node -> middle node 2
        if self.r2 is not None:
            tnode = f"{self.internal_name}mn2"
            circuit.R(
                f"{self.internal_name}r2",
                f"{self.internal_name}mn1",
                tnode,
                self.r2,
            )

        # c: from mn1 or mn2 to gnd
        circuit.C(f"{self.internal_name}c", tnode, circuit.gnd, self.c)

        vco_exp = f"1"
        circuit.B(f"{self.internal_name}b", pco_node, circuit.gnd, v=vco_exp)


# VCO tuning from tnode, output to pco_node
