def make_symbol(relay0: bool, relay1: bool, relay2: bool, tuning: int):
    if tuning > 0b0000_1111_1111_1111 or tuning < 0:
        raise RuntimeError("Tuning must fit in 12 bit")

    data = tuning
    if relay0:
        data |= 0b0001_0000_0000_0000
    if relay1:
        data |= 0b0010_0000_0000_0000
    if relay2:
        data |= 0b0100_0000_0000_0000

    parity = data.bit_count()
    if parity % 2 != 0:
        # Insert the parity bit
        data |= 0b1000_0000_0000_0000

    return data
