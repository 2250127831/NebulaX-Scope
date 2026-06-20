#include "SharedMemoryMonitor.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <chrono>

#include <QDebug>

using Clock = std::chrono::steady_clock;

// ── Shared memory layout ───────────────────────────────────────────────────
static constexpr size_t METRICS_SIZE = 128;

struct SharedMetrics {
    uint64_t io_pid, send_pid;
    uint64_t recv_frames, new_orders, cancels, book_queries;
    uint64_t trades, errors, order_pool_used, order_pool_capacity;
    uint64_t tick_counter;
    uint64_t send_batches, send_bytes, send_zc_ok, send_zc_fail;
    uint64_t send_tick_counter;
};

// ── Construction / destruction ──────────────────────────────────────────────

SharedMemoryMonitor::SharedMemoryMonitor(QObject *parent)
    : QObject(parent) {}

SharedMemoryMonitor::~SharedMemoryMonitor() { stop(); }

// ── Public API ──────────────────────────────────────────────────────────────

bool SharedMemoryMonitor::start() {
    int fd = shm_open("/nebulaX_metrics", O_RDONLY, 0);
    if (fd < 0) { qWarning() << "[Scope] nebulaX_metrics not found"; return false; }
    metrics_ptr_ = mmap(nullptr, METRICS_SIZE, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    if (metrics_ptr_ == MAP_FAILED) { qWarning() << "[Scope] mmap(metrics) failed"; return false; }

    auto* m = static_cast<SharedMetrics*>(metrics_ptr_);
    io_pid_   = m->io_pid;
    send_pid_ = m->send_pid;

    int64_t now = Clock::now().time_since_epoch().count() / 1000;
    last_tick_      = m->tick_counter;
    last_send_tick_ = m->send_tick_counter;
    last_orders_    = m->new_orders;
    last_trades_    = m->trades;
    last_send_bytes_= m->send_bytes;
    io_miss_  = now;
    send_miss_ = now;

    // ── ring shm ──
    fd = shm_open("/nebulaX_ring", O_RDONLY, 0);
    if (fd < 0) { qWarning() << "[Scope] nebulaX_ring not found"; }
    else {
        ring_ptr_ = mmap(nullptr, 24, PROT_READ, MAP_SHARED, fd, 0);
        close(fd);
        if (ring_ptr_ == MAP_FAILED) ring_ptr_ = nullptr;
        else ring_ = static_cast<volatile RingShm*>(ring_ptr_);
    }

    connected_ = true;
    start_ts_us_ = Clock::now().time_since_epoch().count() / 1000;
    last_ts_us_  = start_ts_us_;
    emit connectedChanged();

    // ── Poll timer (120 Hz) ──
    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &SharedMemoryMonitor::poll);
    timer_->start(8);

    // ── Stop reconnect timer if it was running ──
    if (reconnect_) { reconnect_->stop(); reconnect_->deleteLater(); reconnect_ = nullptr; }

    qDebug() << "[Scope] connected";
    return true;
}

void SharedMemoryMonitor::stop() {
    if (timer_) { timer_->stop(); timer_->deleteLater(); timer_ = nullptr; }
    if (metrics_ptr_) { munmap(metrics_ptr_, METRICS_SIZE); metrics_ptr_ = nullptr; }
    if (ring_ptr_) { munmap(ring_ptr_, 24); ring_ptr_ = nullptr; ring_ = nullptr; }
    if (connected_) { connected_ = false; emit connectedChanged(); }
}

// ── Auto-reconnect ─────────────────────────────────────────────────────────

void SharedMemoryMonitor::tryReconnect() {
    if (!connected_ && start()) {
        qDebug() << "[Scope] reconnected";
        if (reconnect_) { reconnect_->stop(); reconnect_->deleteLater(); reconnect_ = nullptr; }
    }
}

// ── Poll (8 ms, 120 Hz) ────────────────────────────────────────────────────

void SharedMemoryMonitor::poll() {
    if (!metrics_ptr_) {
        // Connection lost — start reconnect timer if not already running
        if (!reconnect_) {
            reconnect_ = new QTimer(this);
            connect(reconnect_, &QTimer::timeout, this, &SharedMemoryMonitor::tryReconnect);
            reconnect_->start(2000);
        }
        return;
    }

    int64_t now_us = Clock::now().time_since_epoch().count() / 1000;

    // ── Read raw counters ──
    auto* m = static_cast<SharedMetrics*>(metrics_ptr_);
    uint64_t orders  = m->new_orders;
    uint64_t trades  = m->trades;
    uint64_t errs    = m->errors;
    uint64_t pu      = m->order_pool_used;
    uint64_t pc      = m->order_pool_capacity;
    uint64_t sbytes  = m->send_bytes;
    uint64_t zc_ok   = m->send_zc_ok;
    uint64_t zc_fail = m->send_zc_fail;
    uint64_t tick    = m->tick_counter;
    uint64_t stick   = m->send_tick_counter;

    // ── Ring shm ──
    if (ring_) {
        ring_tail_ = ring_->tail;
        ring_head_ = ring_->head;
        ring_cap_  = ring_->capacity;
        if (ring_cap_ > 0) {
            ring_used_ = (ring_tail_ >= ring_head_)
                ? ring_tail_ - ring_head_
                : ring_tail_ + ring_cap_ - ring_head_;
            ring_util_ = static_cast<double>(ring_used_) / ring_cap_;
        }
    }

    if (first_poll_) {
        qps_ = 0; order_rate_ = 0; trade_rate_ = 0; send_bps_ = 0;
        first_poll_ = false;
    }

    // ── Non-rate values ──
    orders_  = orders;
    trades_  = trades;
    errs_    = errs;
    pool_used_ = pu;
    pool_cap_  = pc;
    pool_util_ = pc > 0 ? static_cast<double>(pu) / pc : 0.0;
    zc_rate_   = (zc_ok + zc_fail) > 0
        ? static_cast<double>(zc_ok) / (zc_ok + zc_fail) : 0.0;

    // ── Thread liveness (3 s wall-clock) ──
    static constexpr int64_t LIVENESS_US = 3'000'000;
    if (tick != last_tick_)  { io_alive_ = true;  io_miss_  = now_us; }
    else                     { io_alive_ = (now_us - io_miss_) < LIVENESS_US; }
    if (stick != last_send_tick_) { send_alive_ = true;  send_miss_ = now_us; }
    else                          { send_alive_ = (now_us - send_miss_) < LIVENESS_US; }

    // ── Uptime ──
    if (start_ts_us_ > 0)
        uptime_ = static_cast<int>((now_us - start_ts_us_) / 1'000'000);

    // ── Ring + pool — 120 Hz ──
    emit ringUpdated();
    emit poolUpdated();

    // ── Text metrics — 4 Hz ──
    if (++ui_update_div_ >= 30) {
        double dt = static_cast<double>(now_us - last_ts_us_) / 1e6;
        if (dt > 0.0) {
            qps_        = static_cast<double>(orders - last_orders_) / dt;
            order_rate_ = static_cast<double>(orders - last_orders_) / dt;
            trade_rate_ = static_cast<double>(trades - last_trades_) / dt;
            send_bps_   = static_cast<double>(sbytes - last_send_bytes_) / dt;
        }
        last_orders_     = orders;
        last_trades_     = trades;
        last_send_bytes_ = sbytes;
        last_ts_us_      = now_us;
        emit updated();
        ui_update_div_ = 0;
    }

    // ── Tick snapshots (per poll) ──
    last_tick_      = tick;
    last_send_tick_ = stick;
}
