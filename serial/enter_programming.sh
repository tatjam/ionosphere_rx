#!/bin/bash

pushd "$(dirname "${BASH_SOURCE[0]}")" > /dev/null

uv run python3 main.py --port /dev/ttyUSB0 --zmq_host tcp://127.0.0.1:9000 &
PID=$!

uv run python3 zmq_tool.py tcp://127.0.0.1:9000 '{"type": "enter_programmer"}'

kill $PID
wait $PID 2>/dev/null
sleep 3

popd > /dev/null
