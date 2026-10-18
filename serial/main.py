#!/usr/bin/env python3
# Send link commands to the pc_interface firmware over a ZeroMQ port
import struct
from json import JSONDecodeError
import os
import logging
import argparse

from utils import fletcher16, cobs, u16, u32
import sys
import time
import zmq
import coloredlogs

import serial

(
    NO_OP,
    ENTER_PROGRAMMER,
    START_SEQUENCE,
    APPEND_SEQUENCE,
    QUERY_SYMBOL_QUEUE,
    SET_CLOCK,
    GET_CLOCK,
) = range(7)


def command(kind, data=b""):
    payload = bytes([kind]) + data
    cks = fletcher16(payload)
    return b"\x00" + cobs(payload + bytes([cks & 0xFF, cks >> 8])) + b"\x00"


def symbols(args):
    assert 0 < len(args) <= 100, "1..100 symbols per message"
    return bytes([len(args)]) + b"".join(u16(int(s)) for s in args)


CMDS = {
    "no_op": lambda a: command(NO_OP),
    "enter_programmer": lambda a: command(ENTER_PROGRAMMER),
    "get_clock": lambda a: command(GET_CLOCK),
    "query_queue": lambda a: command(QUERY_SYMBOL_QUEUE),
    "set_clock": lambda a: command(SET_CLOCK, u32(int(a[0])) + u16(int(a[1]))),
    "start_sequence": lambda a: command(
        START_SEQUENCE,
        bytes([len(a) - 2]) + u32(int(a[0])) + u16(int(a[1])) + symbols(a[2:]),
    ),
    "append_sequence": lambda a: command(APPEND_SEQUENCE, symbols(a)),
}


def do_no_op(serial: serial.SerialBase, inv):
    serial.write(command(NO_OP))
    reply = serial.read()
    assert isinstance(reply, bytes)
    return reply.decode()


def do_enter_programmer(serial: serial.SerialBase, inv):
    serial.write(command(ENTER_PROGRAMMER))
    reply = serial.read()
    assert isinstance(reply, bytes)
    return reply.decode()


def do_get_clock(serial: serial.SerialBase, inv):
    serial.write(command(GET_CLOCK))

    # 'G', then u16 tick, then u32 second (both little-endian)
    reply = serial.read(7)  # read(n) blocks until n bytes or the port timeout
    if len(reply) != 7:
        raise TimeoutError(f"GET_CLOCK: expected 7 bytes, got {len(reply)}")

    if reply[0:1] != b"G":
        raise ValueError(f"GET_CLOCK: bad reply byte {reply[0:1]!r}")

    tick, second = struct.unpack("<HI", reply[1:])
    return second, tick


def do_raw_bytes(serial: serial.SerialBase, inv):
    b = inv.get("byte")
    if isinstance(b, int) and not isinstance(b, bool):
        data = bytes([b])
    elif isinstance(b, list) and all(isinstance(x, int) for x in b):
        data = bytes(b)  # [81, 32] -> b"Q "
    else:
        raise RuntimeError("Expected int or list of ints in key 'byte'")

    serial.write(data)
    serial.flush()

    n = inv.get("await_reply")
    if n is True:
        n = 1
    if isinstance(n, int) and n > 0:
        reply = serial.read(n)
        return list(reply)  # e.g. [20, 16] = 0x14 0x10
    return {}


def do_test_programmer(serial: serial.SerialBase, inv):
    """True if the device is in programming mode (answers STK500 sync)."""
    serial.write(bytes([ord("0"), 0x20]))
    serial.flush()
    reply = serial.read(2)  # returns fewer bytes on timeout
    return reply == bytes([0x14, 0x10])


def do_exit_programmer(serial: serial.SerialBase, inv):
    """Send 'Q' + EOP. Device replies 0x14 0x10, then resets into link mode."""
    serial.write(bytes([ord("Q"), 0x20]))
    serial.flush()
    reply = serial.read(2)
    if reply != bytes([0x14, 0x10]):
        raise RuntimeError(f"EXIT_PROGRAMMER: unexpected reply {list(reply)}")
    return "ok"


def receive(sock):
    inv = sock.recv_json()
    logging.debug(f"Received {inv}")
    return inv


def response(sock, v):
    logging.debug(f"Replying {v}")
    sock.send_json(v)


def rx_loop(serial, rep: zmq.Socket[bytes]):
    while True:
        try:
            inv = receive(rep)
        except JSONDecodeError:
            response(rep, {"ok": False, "err": "Malformed message"})
            continue

        if not isinstance(inv, dict) or "type" not in inv:
            response(
                rep, {"ok": False, "err": "Missing message type or malformed message"}
            )
            continue

        msg_type = inv["type"]
        try:
            if msg_type == "no_op":
                reply = do_no_op(serial, inv)
            elif msg_type == "enter_programmer":
                reply = do_enter_programmer(serial, inv)
            elif msg_type == "get_clock":
                reply = do_get_clock(serial, inv)
            # Custom commands for special uses
            elif msg_type == "raw_bytes":
                reply = do_raw_bytes(serial, inv)
            elif msg_type == "test_programmer":
                reply = do_test_programmer(serial, inv)
            elif msg_type == "exit_programmer":
                reply = do_exit_programmer(serial, inv)
            else:
                raise ValueError(f"Unknown message type {msg_type}")

        except (RuntimeError, ValueError, TypeError) as e:
            response(rep, {"ok": False, "err": f"Error: {e}"})
            continue

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
        help="Serial port, defaults to tcp://127.0.0.1:9000",
    )

    args = parser.parse_args()

    ctx = zmq.Context()
    rep = ctx.socket(zmq.REP)
    rep.bind(args.zmq_host)

    s = None
    while s is None:
        try:
            with serial.Serial(args.port, 250000) as s:
                # Wait for Arduino restart, remove if we move away from Arduino :)
                time.sleep(2)
                logging.info(f"Connection achieved with {args.port}")

                rx_loop(s, rep)

        except serial.SerialException:
            logging.error("Serial error, retrying connection...")

        logging.info(f"pc_interface not connected to {args.port}, retrying...")
        time.sleep(1)


if __name__ == "__main__":
    main()
