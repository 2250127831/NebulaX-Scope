#!/usr/bin/env python3
"""
Send N NEW SELL at a given price, with optional rate limit.
Usage:  python3 sell.py <port> <price> [count] [rate]
        rate=0 (default) means no limit
"""
import socket, struct, time, sys, os, threading

PORT  = int(sys.argv[1]) if len(sys.argv) > 1 else 2250
PRICE = int(sys.argv[2]) if len(sys.argv) > 2 else 100
N     = int(sys.argv[3]) if len(sys.argv) > 3 else 1000
RATE  = int(sys.argv[4]) if len(sys.argv) > 4 else 0   # orders/s, 0=unlimited
UID_START = 2_000_000_000

def cmd_new(price: int, uid: int) -> bytes:
    return struct.pack("<BBxxII4xQQ", 1, 0x02, price, 10, uid, 0)

fd = os.open("/dev/shm/nebulaX_metrics", os.O_RDONLY)
data = os.read(fd, 128); os.close(fd)
v0 = struct.unpack("<16Q", data)
print(f"Before: orders={v0[3]} pool={v0[8]}")

s = socket.socket(); s.settimeout(5)
s.connect(("127.0.0.1", PORT))
s.setblocking(0)
try:
    while True: s.recv(48)
except: pass
s.setblocking(1); s.settimeout(5)

done = threading.Event()
def recv_loop():
    while not done.is_set():
        try:
            s.recv(48)
        except:
            pass
rt = threading.Thread(target=recv_loop, daemon=True)
rt.start()

if RATE > 0:
    print(f"SELL {N} @ {PRICE}  rate={RATE}/s …", flush=True)
else:
    print(f"SELL {N} @ {PRICE} …", flush=True)
t0 = time.time()
interval = 1.0 / RATE if RATE > 0 else 0
for i in range(N):
    s.send(cmd_new(PRICE, UID_START + i))
    if interval:
        time.sleep(interval)
time.sleep(5)
done.set()
s.close()
elapsed = time.time() - t0

fd = os.open("/dev/shm/nebulaX_metrics", os.O_RDONLY)
data = os.read(fd, 128); os.close(fd)
v1 = struct.unpack("<16Q", data)
print(f"After:  orders={v1[3]} pool={v1[8]}  {elapsed*1000:.0f} ms  Δpool={v1[8]-v0[8]}")
