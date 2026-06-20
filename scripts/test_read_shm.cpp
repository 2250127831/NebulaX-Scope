/*
 * Quick test: read shared memory directly and print all metrics.
 * Build: g++ -std=c++17 -o /tmp/test_read_shm test_read_shm.cpp -lrt
 * Run after NebulaX is started.
 */
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <cstdint>
#include <cstddef>

static constexpr size_t METRICS_SIZE = 128;
static constexpr size_t ORDERS_SIZE  = (4UL << 20) * 64;    // 256 MB
static constexpr size_t TRADES_SIZE  = (1UL << 20) * 64;    // 64 MB  (64 B/entry, not 84)
static constexpr size_t META_OFFSET  = ORDERS_SIZE + TRADES_SIZE;  // 320 MB

struct IOCounters {
    uint64_t recv_frames, new_orders, cancels, book_queries;
    uint64_t trades, errors, order_pool_used, order_pool_capacity;
};
struct SendCounters {
    uint64_t send_batches, send_bytes, send_zc_ok, send_zc_fail;
};
struct SharedMetrics {
    uint64_t io_pid, send_pid;
    IOCounters io;
    SendCounters send;
};
struct BookMeta { uint64_t io_heartbeat, send_heartbeat, last_wal_seq; };

int main() {
    // Read metrics
    int fd = shm_open("/nebulaX_metrics", O_RDONLY, 0);
    if (fd < 0) { perror("shm_open metrics"); return 1; }
    auto* m = (SharedMetrics*)mmap(nullptr, METRICS_SIZE, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    if (m == MAP_FAILED) { perror("mmap metrics"); return 1; }

    // Read book heartbeat region (tail of file, not whole 320 MB)
    fd = shm_open("/nebulaX_book", O_RDONLY, 0);
    if (fd < 0) { perror("shm_open book"); return 1; }

    off_t file_sz = lseek(fd, 0, SEEK_END);
    if (file_sz < (off_t)(META_OFFSET + 24)) {
        fprintf(stderr, "File too small: %jd (need at least %zu)\n",
                (intmax_t)file_sz, META_OFFSET + 24);
        close(fd); return 1;
    }
    size_t tail_len = file_sz - META_OFFSET;
    auto* meta = (BookMeta*)mmap(nullptr, tail_len, PROT_READ, MAP_SHARED, fd, META_OFFSET);
    close(fd);
    if (meta == MAP_FAILED) { perror("mmap book meta"); return 1; }

    printf("═══ NebulaX Shared Memory Dump ═══\n\n");

    printf("Threads:\n");
    printf("  IO   PID: %lu\n",  m->io_pid);
    printf("  Send PID: %lu\n\n", m->send_pid);

    printf("IOCounters:\n");
    printf("  recv_frames:       %lu\n", m->io.recv_frames);
    printf("  new_orders:        %lu\n", m->io.new_orders);
    printf("  cancels:           %lu\n", m->io.cancels);
    printf("  book_queries:      %lu\n", m->io.book_queries);
    printf("  trades:            %lu\n", m->io.trades);
    printf("  errors:            %lu\n", m->io.errors);
    printf("  order_pool_used:   %lu\n", m->io.order_pool_used);
    printf("  order_pool_cap:    %lu\n", m->io.order_pool_capacity);

    double pool_util = m->io.order_pool_capacity > 0
        ? (double)m->io.order_pool_used / m->io.order_pool_capacity : 0;
    printf("  pool_utilization:  %.1f%%\n\n", pool_util * 100);

    printf("SendCounters:\n");
    printf("  send_batches:      %lu\n", m->send.send_batches);
    printf("  send_bytes:        %lu\n", m->send.send_bytes);
    printf("  send_zc_ok:        %lu\n", m->send.send_zc_ok);
    printf("  send_zc_fail:      %lu\n", m->send.send_zc_fail);

    double zc_rate = (m->send.send_zc_ok + m->send.send_zc_fail) > 0
        ? (double)m->send.send_zc_ok / (m->send.send_zc_ok + m->send.send_zc_fail) : 0;
    printf("  zc_success_rate:   %.1f%%\n\n", zc_rate * 100);

    printf("BookMeta:\n");
    printf("  io_heartbeat:      %lu\n", meta->io_heartbeat);
    printf("  send_heartbeat:    %lu\n", meta->send_heartbeat);
    printf("  last_wal_seq:      %lu\n", meta->last_wal_seq);

    munmap(m, METRICS_SIZE);
    munmap((void*)meta, tail_len);
    return 0;
}
