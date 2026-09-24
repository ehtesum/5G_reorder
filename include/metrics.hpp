#pragma once

#include <cstdint>
#include <atomic>
#include <string>
#include <sstream>

namespace pdcp {

struct EngineMetrics {
    std::atomic<uint64_t> packets_received{0};
    std::atomic<uint64_t> packets_delivered_in_order{0};
    std::atomic<uint64_t> out_of_order_buffered{0};
    std::atomic<uint64_t> duplicate_dropped{0};
    std::atomic<uint64_t> stale_dropped{0};
    std::atomic<uint64_t> timer_flushes{0};
    std::atomic<uint32_t> current_buffer_size{0};

    std::string summary() const {
        std::ostringstream oss;
        oss << "rx=" << packets_received.load()
            << " delivered=" << packets_delivered_in_order.load()
            << " buffered=" << out_of_order_buffered.load()
            << " dup_dropped=" << duplicate_dropped.load()
            << " stale_dropped=" << stale_dropped.load()
            << " timer_flushes=" << timer_flushes.load()
            << " active_buf=" << current_buffer_size.load();
        return oss.str();
    }
};

} // namespace pdcp
