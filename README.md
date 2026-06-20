# NebulaX-Scope

[![Qt](https://img.shields.io/badge/Qt-6.2-41CD52?logo=qt)](https://www.qt.io)
[![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=c%2B%2B)](https://isocpp.org)
[![License](https://img.shields.io/badge/license-MIT-blue)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Linux-FFD700?logo=linux)](https://kernel.org)

**Real-time shared-memory telemetry dashboard for high-performance trading systems.**

NebulaX-Scope is a standalone monitoring tool that reads `/dev/shm` shared memory from [NebulaX](https://github.com/user/NebulaX) — a low-latency exchange matching engine — and visualises internal metrics via a GPU-accelerated Qt6 QML interface.

> **Zero-instrumentation.** No changes to the monitored service. No agents, no IPC, no network overhead.
> Read-only `mmap` of 152 bytes across two shared-memory files — the rest is derived metrics.

---

## Dashboard

```
┌──────────────────────────────────────────────────────────────┐
│  NebulaX Scope ● RUNNING                        ─  ✕        │
│  Telemetry Dashboard                                         │
├──────────┬──────────┬──────────┬──────────┬──────────────────┤
│   QPS    │ TRADES/s │  OUTPUT  │ ORDER/s  │    ERRORS        │
│ 152,334  │ 148,771  │ 1.82 MB/s│ 5,000    │       0          │
├──────────┴──────────┴──────────┴──────────┴──────────────────┤
│                      SPSC RING                                │
│                  ╭──────────╮  52%                            │
│                 ╱           ╲  4.2 / 8.0 MB                   │
│                │             │                                │
│                 ╲           ╱                                 │
│                  ╰──────────╯                                 │
├──────────────────────────────────────────────────────────────┤
│                     ORDER POOL                                │
│  ████████░░░░░░░░░░░░░░░░░░░░░░  3.1%                        │
│  132,046 / 4,194,304                                         │
├──────────────────────────────────────────────────────────────┤
│  ● IO PID 1234   ● Send PID 5678            UP 42s    120Hz  │
└──────────────────────────────────────────────────────────────┘
```

---

## Architecture

```
 ┌─────────────────────────────────────────────────────────────┐
 │                    NebulaX (C++)                             │
 │  writes: /dev/shm/nebulaX_metrics  (128 B, 16 × uint64_t)  │
 │          /dev/shm/nebulaX_ring     ( 24 B,  3 × uint64_t)  │
 └────────────────────┬────────────────────────────────────────┘
                      │ mmap (read-only)
                      ▼
 ┌─────────────────────────────────────────────────────────────┐
 │  SharedMemoryMonitor (QObject, QML singleton)               │
 │  ┌───────────────────────────────────────────────────────┐  │
 │  │ QTimer 8 ms (120 Hz) → poll() -> emit signals        │  │
 │  │   ├── ringUpdated()   →  RingTelemetry (Canvas)       │  │
 │  │   ├── poolUpdated()   →  OrderPoolBar (progress bar)  │  │
 │  │   └── updated()       →  MetricCard (text, 4 Hz)      │  │
 │  └───────────────────────────────────────────────────────┘  │
 └────────────────────┬────────────────────────────────────────┘
                      │ QML binding (declarative, GPU-scene-graph)
                      ▼
 ┌─────────────────────────────────────────────────────────────┐
 │  ScopeWindow.qml (ApplicationWindow, Frameless)             │
 │  ├── MetricCard.qml     (smooth number animation)           │
 │  ├── RingTelemetry.qml  (Canvas ring + breathing glow)      │
 │  ├── OrderPoolBar.qml   (progress bar + shimmer)            │
 │  └── Status bar         (thread liveness, PID, uptime)      │
 └─────────────────────────────────────────────────────────────┘
```

### Key design decisions

| Decision | Rationale |
|----------|-----------|
| **Shared memory** vs TCP/UDP | Zero-copy, no context switch, ~50 ns latency |
| **`mmap` only 152 B** (not 340 MB book file) | Avoids faulting in pages from the giant order-book shm |
| **C++ backend, QML frontend** | Qt6 declarative bindings eliminate glue code |
| **3 independent update signals** | Ring (120 Hz) / Pool (120 Hz) / Text (4 Hz) — no wasteful repaints |
| **Canvas 2D** vs Qt Charts / Shapes | Qt 6.2 Shapes has sweep-angle bugs; Canvas is reliable and well-tested |
| **Auto-reconnect** | Poll every 2 s when NebulaX restarts — no manual relaunch |

---

## Shared Memory Layout

### `/dev/shm/nebulaX_metrics` — 128 bytes

| Offset | Field | Source | Description |
|--------|-------|--------|-------------|
| 0 | `io_thread_pid` | main | IO thread PID (set once at startup) |
| 1 | `send_thread_pid` | main | Send thread PID |
| 2 | `recv_frames` | IO | Completed recv CQEs |
| 3 | `new_orders` | IO | New order count |
| 4 | `cancels` | IO | Cancel count |
| 5 | `book_queries` | IO | Order-book query count |
| 6 | `trades` | IO | Trade count |
| 7 | `errors` | IO | Error count |
| 8 | `order_pool_used` | IO | Live orders in pool |
| 9 | `order_pool_capacity` | IO | Pool capacity (fixed 4M) |
| 10 | `tick_counter` | IO | Incremented each IO loop |
| 11 | `send_batches` | Send | Send batch count |
| 12 | `send_bytes` | Send | Total bytes sent |
| 13 | `send_zc_ok` | Send | Zero-copy success count |
| 14 | `send_zc_fail` | Send | Zero-copy fallback count |
| 15 | `send_tick_counter` | Send | Incremented each Send loop |

```python
import mmap, os, struct
fd = os.open("/dev/shm/nebulaX_metrics", os.O_RDONLY)
mm = mmap.mmap(fd, 128, mmap.MAP_SHARED, mmap.PROT_READ)
io_pid, send_pid, _, orders, *_ = struct.unpack_from("<16Q", mm, 0)
```

### `/dev/shm/nebulaX_ring` — 24 bytes

| Offset | Field | Description |
|--------|-------|-------------|
| 0 | `tail` | IO thread write position (ring-relative offset 0..capacity) |
| 1 | `head` | Send thread read position |
| 2 | `capacity` | Ring capacity (fixed 8 MB) |

Derived: `used = tail - head` (mod capacity), `utilisation = used / capacity`

---

## Build & Run

### Prerequisites

```bash
sudo apt install qt6-base-dev qt6-declarative-dev
```

### Build

```bash
cd NebulaX-Scope
rm -rf build && mkdir build && cd build
cmake .. && make -j$(nproc)
```

### Run

1. Start NebulaX:

```bash
taskset -c 6,7 /path/to/nebulaX 2250 --io-core 6 --send-core 7 &
```

2. Launch Scope:

```bash
./build/scope
```

The dashboard auto-connects to shared memory and displays live metrics. Reconnects automatically if NebulaX restarts.

---

## Backend API Reference (QML)

The C++ class `SharedMemoryMonitor` is registered as the QML singleton `Monitor` (module `NebulaX.Scope`).

```qml
import NebulaX.Scope 1.0

Text { text: Monitor.qps }                 // real-time QPS
Text { text: Monitor.ioAlive }             // IO thread health
```

### Properties

| Property | Type | Signal | Rate | Description |
|----------|------|--------|------|-------------|
| `qps` | double | `updated` | 4 Hz | Orders per second |
| `orderRate` | double | `updated` | 4 Hz | New order rate |
| `tradeRate` | double | `updated` | 4 Hz | Trade rate |
| `sendThroughput` | double | `updated` | 4 Hz | Send throughput (bytes/s) |
| `zcRate` | double | `updated` | 4 Hz | Zero-copy success rate |
| `totalOrders` | uint64 | `updated` | 4 Hz | Cumulative orders |
| `totalTrades` | uint64 | `updated` | 4 Hz | Cumulative trades |
| `errors` | uint64 | `updated` | 4 Hz | Error count |
| `ioAlive` | bool | `updated` | 4 Hz | IO thread healthy |
| `sendAlive` | bool | `updated` | 4 Hz | Send thread healthy |
| `ioPid` | uint64 | `updated` | 4 Hz | IO thread PID |
| `sendPid` | uint64 | `updated` | 4 Hz | Send thread PID |
| `uptimeSeconds` | int | `updated` | 4 Hz | Session uptime |
| `poolUtilization` | double | `poolUpdated` | 120 Hz | Order pool fill ratio |
| `orderPoolUsed` | uint64 | `poolUpdated` | 120 Hz | Live pool entries |
| `orderPoolCapacity` | uint64 | `poolUpdated` | 120 Hz | Pool capacity (4,194,304) |
| `ringReadPtr` | uint64 | `ringUpdated` | 120 Hz | Ring tail (IO write pos) |
| `ringWritePtr` | uint64 | `ringUpdated` | 120 Hz | Ring head (Send read pos) |
| `ringUsedBytes` | uint64 | `ringUpdated` | 120 Hz | Bytes occupied |
| `ringCapacity` | uint64 | `ringUpdated` | 120 Hz | Ring capacity (8,388,608) |
| `ringUtilization` | double | `ringUpdated` | 120 Hz | Ring fill ratio |
| `connected` | bool | `connectedChanged` | event | Shared memory reachable |

### Signals

```qml
Monitor.updated()            // text metrics refreshed
Monitor.poolUpdated()        // pool bar refreshed
Monitor.ringUpdated()        // ring canvas refreshed
Monitor.connectedChanged()   // connection status changed
```

---

## Scripts

| Script | Purpose |
|--------|---------|
| [`buy.py`](scripts/buy.py) | Send N BUY orders at a given price |
| [`sell.py`](scripts/sell.py) | Send N SELL orders at a given price |
| [`bench_backpressure.py`](scripts/bench_backpressure.py) | Sustained ring pressure test with adaptive back-pressure |
| [`test_read_shm.cpp`](scripts/test_read_shm.cpp) | Minimal C++ shared-memory reader (no Qt) |

---

## Derived Metrics

| Metric | Formula | Source |
|--------|---------|--------|
| QPS | `Δorders / Δt` | `new_orders` |
| Trade rate | `Δtrades / Δt` | `trades` |
| Send throughput | `Δbytes / Δt` | `send_bytes` |
| ZC success rate | `zc_ok / (zc_ok + zc_fail)` | `send_zc_ok/fail` |
| Pool utilisation | `pool_used / pool_capacity` | `order_pool_used/capacity` |
| Ring utilisation | `used / capacity` | `tail, head` |
| IO alive | `tick_counter` changed in < 3 s | `tick_counter` |
| Send alive | `send_tick_counter` changed in < 3 s | `send_tick_counter` |

---

## Project Structure

```
NebulaX-Scope/
├── CMakeLists.txt                # qt_add_qml_module declarative build
├── README.md
├── qml/
│   ├── ScopeWindow.qml           # Main window (frameless, rounded)
│   ├── MetricCard.qml            # Metric card with smooth number animation
│   ├── RingTelemetry.qml         # Canvas ring with breathing glow
│   └── OrderPoolBar.qml          # Progress bar with shimmer
├── src/
│   ├── main.cpp                  # Entry point, singleton registration
│   ├── SharedMemoryMonitor.h     # 22 Q_PROPERTY declarations
│   └── SharedMemoryMonitor.cpp   # mmap, poll, reconnect logic
└── scripts/
    ├── buy.py                    # Order injection tool
    ├── sell.py                   # Order injection tool
    ├── bench_backpressure.py     # Ring pressure test
    └── test_read_shm.cpp         # Standalone shm reader (no Qt)
```

---

## Why not …?

**Qt Charts?** — Avoids an extra dependency; Canvas gives full control over rendering and is already GPU-accelerated via the Qt Quick scene graph when used with the `rhi` backend.

**Web-based dashboard?** — A web frontend (Grafana, etc.) would require exposing metrics over HTTP, adding latency and surface area. Shared-memory `mmap` is the lowest-overhead path to the data.

**Widgets?** — QML's declarative bindings (`Q_PROPERTY ... NOTIFY`) eliminate glue code. The same feature set in Widgets would require 3× more C++ for `setText()` calls and timer bookkeeping.

---

## License

MIT
