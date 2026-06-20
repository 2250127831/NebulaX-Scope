#!/usr/bin/env python3
"""
Sustained ring pressure: recv-rate control based on ring utilization.
Usage: python3 bench_backpressure.py [port] [high_threshold%]
  默认: python3 bench_backpressure.py 2250 60
"""
import socket, struct, time, os, mmap, sys

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 2250
HI   = float(sys.argv[2]) if len(sys.argv) > 2 else 60
MID  = HI - 20
LO   = HI - 30

s = socket.socket(); s.settimeout(30)
s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 32768)
s.connect(("127.0.0.1", PORT))
s.setblocking(0)
try:
    while True: s.recv(65536)
except: pass

ring_fd = os.open("/dev/shm/nebulaX_ring", os.O_RDONLY)
ring_mm = mmap.mmap(ring_fd, 24, mmap.MAP_SHARED, mmap.PROT_READ)
os.close(ring_fd)

def snap():
    t, h, cap = struct.unpack_from("<3Q", ring_mm, 0)
    return t, h, t - h, cap

s.setblocking(1)
uid = 1
buf = bytearray()
for i in range(150000):
    side = 0x01 if i % 2 == 0 else 0x02
    buf.extend(struct.pack("<BBxxII4xQQ", 1, side, 100 + i//2, 10, uid + i, 0))
s.sendall(bytes(buf))
t, h, used, cap = snap()
print("fill: used=%d (%.1f%%)" % (used, used*100/cap), flush=True)
print("threshold: >%.0f%%=%dB  >%.0f%%=%dB  <=%.0f%%=%dB" % (HI, 50000, MID, 9600, LO, 2400), flush=True)

s.setblocking(0)
t0 = time.monotonic()
last_print = 0
n_sent = 150000
next_tick = 0.0

while n_sent < 500000:
    now = time.monotonic()
    if now < next_tick:
        time.sleep(0.0005)
        continue
    next_tick = now + 0.008

    buf = bytearray()
    for i in range(80):
        side = 0x01 if i % 2 == 0 else 0x02
        buf.extend(struct.pack("<BBxxII4xQQ", 1, side, 100 + i//2, 10, uid + n_sent + i, 0))
    try:
        s.sendall(bytes(buf))
        n_sent += 50
    except:
        pass

    t, h, used, cap = snap()
    util = used * 100.0 / cap
    if util > HI:
        want = 50000
    elif util > MID:
        want = 9600
    else:
        want = 2400
    while want > 0:
        try:
            c = s.recv(min(want, 65536))
            if not c: break
            want -= len(c)
        except: break

    now = time.monotonic()
    if now - last_print >= 1:
        # print silently — no output to avoid clutter in background mode
        last_print = now

t, h, used, cap = snap()
print("done: used=%d (%.1f%%)" % (used, used*100/cap), flush=True)
