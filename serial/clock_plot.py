# Use as
# uv run python3 clock_plot.py | head -n 30 | uplot line -t "MCU - PC (ms)" --xlabel "s" --ylabel "ms"
import signal
import sys, time, zmq

signal.signal(signal.SIGPIPE, signal.SIG_DFL)  #

s = zmq.Context().socket(zmq.REQ)
s.setsockopt(zmq.RCVTIMEO, 3000)
s.connect("tcp://127.0.0.1:9000")

t0 = o = None
while True:
    a = time.perf_counter()
    s.send_json({"type": "get_clock"})
    sec, tick = s.recv_json()["reply"]
    b = time.perf_counter()
    pc, mcu = (a + b) / 2, sec + tick / 62500  # midpoint cancels ~half the latency
    if t0 is None:
        t0, o = pc, mcu - pc
    print(f"{pc - t0:.1f}\t{(mcu - pc - o) * 1000:.3f}", flush=True)
    time.sleep(1)
