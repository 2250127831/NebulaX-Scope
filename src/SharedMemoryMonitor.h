#pragma once

#include <QObject>
#include <QTimer>
#include <cstdint>

class SharedMemoryMonitor : public QObject {
    Q_OBJECT
    // ── Core metrics ──
    Q_PROPERTY(double qps READ qps NOTIFY updated)
    Q_PROPERTY(double orderRate READ orderRate NOTIFY updated)
    Q_PROPERTY(double tradeRate READ tradeRate NOTIFY updated)
    Q_PROPERTY(uint64_t totalOrders READ totalOrders NOTIFY updated)
    Q_PROPERTY(uint64_t totalTrades READ totalTrades NOTIFY updated)
    Q_PROPERTY(uint64_t errors READ errors NOTIFY updated)

    // ── Order pool ──
    Q_PROPERTY(double poolUtilization READ poolUtilization NOTIFY poolUpdated)
    Q_PROPERTY(uint64_t orderPoolUsed READ orderPoolUsed NOTIFY poolUpdated)
    Q_PROPERTY(uint64_t orderPoolCapacity READ orderPoolCapacity NOTIFY poolUpdated)

    // ── Send / network ──
    Q_PROPERTY(double sendThroughput READ sendThroughput NOTIFY updated)
    Q_PROPERTY(double zcRate READ zcRate NOTIFY updated)

    // ── Ring buffer ──
    Q_PROPERTY(uint64_t ringReadPtr READ ringReadPtr NOTIFY ringUpdated)
    Q_PROPERTY(uint64_t ringWritePtr READ ringWritePtr NOTIFY ringUpdated)
    Q_PROPERTY(uint64_t ringUsedBytes READ ringUsedBytes NOTIFY ringUpdated)
    Q_PROPERTY(uint64_t ringCapacity READ ringCapacity NOTIFY ringUpdated)
    Q_PROPERTY(double ringUtilization READ ringUtilization NOTIFY ringUpdated)

    // ── Thread liveness ──
    Q_PROPERTY(bool ioAlive READ ioAlive NOTIFY updated)
    Q_PROPERTY(bool sendAlive READ sendAlive NOTIFY updated)
    Q_PROPERTY(uint64_t ioPid READ ioPid NOTIFY updated)
    Q_PROPERTY(uint64_t sendPid READ sendPid NOTIFY updated)

    // ── Connection ──
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
    Q_PROPERTY(int uptimeSeconds READ uptimeSeconds NOTIFY updated)

public:
    explicit SharedMemoryMonitor(QObject *parent = nullptr);
    ~SharedMemoryMonitor() override;

    double qps() const { return qps_; }
    double orderRate() const { return order_rate_; }
    double tradeRate() const { return trade_rate_; }
    uint64_t totalOrders() const { return orders_; }
    uint64_t totalTrades() const { return trades_; }
    uint64_t errors() const { return errs_; }
    double poolUtilization() const { return pool_util_; }
    uint64_t orderPoolUsed() const { return pool_used_; }
    uint64_t orderPoolCapacity() const { return pool_cap_; }
    double sendThroughput() const { return send_bps_; }
    double zcRate() const { return zc_rate_; }
    uint64_t ringReadPtr() const { return ring_tail_; }
    uint64_t ringWritePtr() const { return ring_head_; }
    uint64_t ringUsedBytes() const { return ring_used_; }
    uint64_t ringCapacity() const { return ring_cap_; }
    double ringUtilization() const { return ring_util_; }
    bool ioAlive() const { return io_alive_; }
    bool sendAlive() const { return send_alive_; }
    bool isConnected() const { return connected_; }
    uint64_t ioPid() const { return io_pid_; }
    uint64_t sendPid() const { return send_pid_; }
    int uptimeSeconds() const { return uptime_; }

    Q_INVOKABLE bool start();
    Q_INVOKABLE void stop();

signals:
    void updated();
    void ringUpdated();
    void poolUpdated();
    void connectedChanged();

private:
    void poll();
    void tryReconnect();

    // ── mmap pointers ──
    void* metrics_ptr_  = nullptr;
    void* ring_ptr_     = nullptr;

    struct RingShm { uint64_t tail, head, capacity; };
    volatile RingShm* ring_ = nullptr;

    QTimer* timer_      = nullptr;
    QTimer* reconnect_  = nullptr;

    // ── Derived values ──
    double qps_ = 0, order_rate_ = 0, trade_rate_ = 0;
    double pool_util_ = 0, send_bps_ = 0, zc_rate_ = 0;
    uint64_t orders_ = 0, trades_ = 0, errs_ = 0;
    uint64_t pool_used_ = 0, pool_cap_ = 0;
    uint64_t io_pid_ = 0, send_pid_ = 0;
    uint64_t ring_tail_ = 0, ring_head_ = 0, ring_cap_ = 0, ring_used_ = 0;
    double ring_util_ = 0;
    bool io_alive_ = false, send_alive_ = false, connected_ = false;

    // ── Delta snapshots ──
    uint64_t last_orders_ = 0, last_trades_ = 0, last_send_bytes_ = 0;
    uint64_t last_tick_ = 0, last_send_tick_ = 0;
    int64_t last_ts_us_ = 0;
    int64_t io_miss_ = 0, send_miss_ = 0;
    int64_t start_ts_us_ = 0;

    bool first_poll_ = true;
    int uptime_ = 0;
    int ui_update_div_ = 0;
};
