#!/usr/bin/env python3
# Send link commands to the pc_interface firmware over a ZeroMQ port
import struct
import os
import logging
import argparse
import time

import zmq
import coloredlogs
import serial

from utils import fletcher16, cobs

BAUD = 250000
DEFAULT_TIMEOUT = 1.0  # seconds

# Command kinds, the values must match the firmware
(
    NO_OP,
    ENTER_PROGRAMMER,
    START_SEQUENCE,
    APPEND_SEQUENCE,
    QUERY_SYMBOL_QUEUE,
    SET_CLOCK,
    GET_CLOCK,
    GET_ERROR,
) = range(8)


class DeviceTimeout(Exception):
    """The device did not answer; the serial link should be reset."""


def command(kind, data=b""):
    payload = bytes([kind]) + data
    cks = fletcher16(payload)
    return b"\x00" + cobs(payload + bytes([cks & 0xFF, cks >> 8])) + b"\x00"


def read_exact(ser, n, what):
    """Read exactly n bytes or raise DeviceTimeout."""
    data = ser.read(n)
    if len(data) != n:
        raise DeviceTimeout(f"{what}: expected {n} byte(s), got {len(data)}")
    return data


def do_no_op(ser, inv):
    ser.write(command(NO_OP))
    return read_exact(ser, 1, "NO_OP").decode()


def do_enter_programmer(ser, inv):
    ser.write(command(ENTER_PROGRAMMER))
    return read_exact(ser, 1, "ENTER_PROGRAMMER").decode()


def do_get_clock(ser, inv):
    ser.write(command(GET_CLOCK))

    # 'G', then u16 tick, then u32 second (both little-endian)
    reply = read_exact(ser, 7, "GET_CLOCK")
    if reply[0:1] != b"G":
        raise ValueError(f"GET_CLOCK: bad reply byte {reply[0:1]!r}")

    tick, second = struct.unpack("<HI", reply[1:])
    return second, tick


def do_raw_bytes(ser, inv):
    b = inv.get("byte")
    if isinstance(b, int) and not isinstance(b, bool):
        data = bytes([b])
    elif isinstance(b, list) and all(isinstance(x, int) for x in b):
        data = bytes(b)  # [81, 32] -> b"Q "
    else:
        raise RuntimeError("Expected int or list of ints in key 'byte'")

    ser.write(data)
    ser.flush()

    n = inv.get("await_reply")
    if n is True:
        n = 1
    if isinstance(n, int) and n > 0:
        return list(read_exact(ser, n, "raw_bytes"))  # e.g. [20, 16]
    return {}


def do_test_programmer(ser, inv):
    """True if the device is in programming mode (answers STK500 sync)."""
    ser.write(bytes([ord("0"), 0x20]))
    ser.flush()
    # A timeout here is a legitimate "no" (link mode), so no reset
    return ser.read(2) == bytes([0x14, 0x10])


def do_exit_programmer(ser, inv):
    """Send 'Q' + EOP. Device replies 0x14 0x10, then resets into link mode."""
    ser.write(bytes([ord("Q"), 0x20]))
    ser.flush()
    reply = read_exact(ser, 2, "EXIT_PROGRAMMER")
    if reply != bytes([0x14, 0x10]):
        raise RuntimeError(f"EXIT_PROGRAMMER: unexpected reply {list(reply)}")
    return "ok"


def do_get_error(ser, inv):
    ser.write(command(GET_ERROR))

    # 'G', then u8 error
    reply = read_exact(ser, 2, "GET_ERROR")
    if reply[0:1] != b"G":
        raise ValueError(f"GET_ERROR: bad reply byte {reply[0:1]!r}")

    error = struct.unpack("<B", reply[1:])[0]
    return error


def _symbols(syms):
    if not isinstance(syms, list) or not all(
        isinstance(s, int) and not isinstance(s, bool) and 0 <= s <= 0xFFFF
        for s in syms
    ):
        raise ValueError("'symbols' must be a list of u16 integers")
    if len(syms) > 255:
        raise ValueError("symbol count must fit in a u8")
    return struct.pack(f"<{len(syms)}H", *syms)


def start_sequence_payload(second, ticks, syms):
    return struct.pack("<BIH", len(syms), second, ticks) + _symbols(syms)


def append_sequence_payload(syms):
    return struct.pack("<B", len(syms)) + _symbols(syms)


def _read_sequence_reply(ser, sent, what):
    reply = read_exact(ser, 2, what)
    if reply[0:1] != b"G":
        raise ValueError(f"{what}: bad reply byte {reply[0:1]!r}")
    accepted = reply[1]
    if accepted != sent:
        raise RuntimeError(
            f"{what}: only {accepted} of {sent} symbols queued (queue full or time too early)"
        )
    return {"accepted": accepted}


def do_start_sequence(ser, inv):
    syms = inv["symbols"]
    data = start_sequence_payload(inv["second"], inv["ticks"], syms)
    ser.write(command(START_SEQUENCE, data))
    return _read_sequence_reply(ser, len(syms), "START_SEQUENCE")


def do_append_sequence(ser, inv):
    syms = inv["symbols"]
    ser.write(command(APPEND_SEQUENCE, append_sequence_payload(syms)))
    return _read_sequence_reply(ser, len(syms), "APPEND_SEQUENCE")


def do_query_symbol_queue(ser, inv):
    ser.write(command(QUERY_SYMBOL_QUEUE))

    # 'G', then u16 remains
    reply = read_exact(ser, 3, "QUERY_SYMBOL_QUEUE")
    if reply[0:1] != b"G":
        raise ValueError(f"QUERY_SYMBOL_QUEUE: bad reply byte {reply[0:1]!r}")

    remain = struct.unpack("<H", reply[1:])[0]
    return remain


HANDLERS = {
    "no_op": do_no_op,
    "enter_programmer": do_enter_programmer,
    "get_clock": do_get_clock,
    "query_symbol_queue": do_query_symbol_queue,
    "get_error": do_get_error,
    "start_sequence": do_start_sequence,
    "append_sequence": do_append_sequence,
    # Custom commands for special uses
    "raw_bytes": do_raw_bytes,
    "test_programmer": do_test_programmer,
    "exit_programmer": do_exit_programmer,
}


def receive(sock):
    inv = sock.recv_json()
    logging.debug(f"Received {inv}")
    return inv


def response(sock, v):
    logging.debug(f"Replying {v}")
    sock.send_json(v)


def rx_loop(ser, rep: zmq.Socket[bytes]):
    while True:
        try:
            inv = receive(rep)
        except ValueError:  # JSONDecodeError and UnicodeDecodeError
            response(rep, {"ok": False, "err": "Malformed message"})
            continue

        if not isinstance(inv, dict) or "type" not in inv:
            response(
                rep, {"ok": False, "err": "Missing message type or malformed message"}
            )
            continue

        try:
            handler = HANDLERS.get(inv["type"])
            if handler is None:
                raise ValueError(f"Unknown message type {inv['type']}")
            reply = handler(ser, inv)

        except (RuntimeError, ValueError, TypeError) as e:
            response(rep, {"ok": False, "err": f"Error: {e}"})
            continue
        except (DeviceTimeout, serial.SerialException) as e:
            # Always answer first: a REP socket that got a request must send a
            # reply, or the next recv fails. Then let main() reset the link.
            response(rep, {"ok": False, "err": f"Device error: {e}"})
            raise

        response(rep, {"ok": True, "reply": reply})


def main():
    coloredlogs.install(level=os.getenv("LOGLEVEL") or "DEBUG")
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--port", default="/dev/ttyUSB0", help="Serial port, defaults to /dev/ttyUSB0"
    )
    parser.add_argument(
        "--zmq_host",
        default="tcp://127.0.0.1:9000",
        help="ZeroMQ address, defaults to tcp://127.0.0.1:9000",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=DEFAULT_TIMEOUT,
        help=f"Serial read/write timeout in seconds, defaults to {DEFAULT_TIMEOUT}",
    )

    args = parser.parse_args()

    ctx = zmq.Context()
    rep = ctx.socket(zmq.REP)
    rep.bind(args.zmq_host)

    while True:
        try:
            with serial.Serial(
                args.port,
                BAUD,
                timeout=args.timeout,
                write_timeout=args.timeout,
            ) as s:
                # Opening the port toggles DTR, which resets the Arduino.
                # Wait for it to restart, remove if we move away from Arduino :)
                time.sleep(2)
                logging.info(f"Connection achieved with {args.port}")

                rx_loop(s, rep)

        except (serial.SerialException, DeviceTimeout) as e:
            logging.error(f"{e}")

        logging.info(f"Resetting connection to {args.port}, retrying...")
        time.sleep(1)


if __name__ == "__main__":
    main()
