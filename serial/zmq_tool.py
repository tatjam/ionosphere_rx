import sys, zmq, json

addr, msg = sys.argv[1], sys.argv[2]
s = zmq.Context().socket(zmq.REQ)
s.setsockopt(zmq.RCVTIMEO, 3000)  # don't hang forever if no reply
s.connect(addr)
s.send_json(json.loads(msg))
print(json.dumps(s.recv_json(), indent=2))
