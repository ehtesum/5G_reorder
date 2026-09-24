#pragma once

#include "packet.hpp"
#include "metrics.hpp"

#include <map>
#include <functional>
#include <chrono>
#include <cstdint>

namespace pdcp {

struct ReorderConfig {
    uint32_t window_size{512};
    std::chrono::milliseconds timer_duration{100};
    bool drop_duplicates{true};
};

class ReorderBuffer {
public:
    using DeliverySink = std::function<void(Packet)>;

private:
    const ReorderConfig config_;
    DeliverySink sink_;
    EngineMetrics metrics_;

    // 3GPP TS 38.323 state variables
    uint32_t rx_deliv_{0};
    uint32_t rx_next_{0};
    uint32_t rx_reord_{0};
    bool initialized_{false};

    bool timer_running_{false};
    std::chrono::steady_clock::time_point timer_start_;

    std::map<uint32_t, Packet> buffer_;

    void flush_contiguous() {
        while (buffer_.count(rx_deliv_)) {
            auto it = buffer_.find(rx_deliv_);
            sink_(std::move(it->second));
            buffer_.erase(it);
            metrics_.packets_delivered_in_order++;
            rx_deliv_++;
        }
        metrics_.current_buffer_size.store(static_cast<uint32_t>(buffer_.size()));

        if (timer_running_ && rx_deliv_ >= rx_reord_) {
            timer_running_ = false;
        }
    }

public:
    explicit ReorderBuffer(ReorderConfig config, DeliverySink sink)
        : config_(config), sink_(std::move(sink)) {}

    bool process_packet(Packet pkt) {
        metrics_.packets_received++;
        const uint32_t sn = pkt.seq_num;

        if (!initialized_) {
            rx_deliv_ = sn;
            rx_next_ = sn;
            initialized_ = true;
        }

        // Stale or duplicate
        if (sn < rx_deliv_) {
            metrics_.stale_dropped++;
            return false;
        }
        if (buffer_.count(sn)) {
            metrics_.duplicate_dropped++;
            return false;
        }

        // In-order expected
        if (sn == rx_deliv_) {
            sink_(std::move(pkt));
            metrics_.packets_delivered_in_order++;
            rx_deliv_++;

            if (rx_deliv_ > rx_next_) {
                rx_next_ = rx_deliv_;
            }
            flush_contiguous();
            return true;
        }

        // Out-of-window check
        if (sn >= rx_deliv_ + config_.window_size) {
            return false;
        }

        // Sequence gap detected: buffer packet
        buffer_[sn] = std::move(pkt);
        metrics_.out_of_order_buffered++;
        metrics_.current_buffer_size.store(static_cast<uint32_t>(buffer_.size()));

        if (sn >= rx_next_) {
            rx_next_ = sn + 1;
        }

        // 3GPP TS 38.323 Clause 5.2.2.2: Start t-Reordering timer
        if (!timer_running_) {
            timer_running_ = true;
            timer_start_ = std::chrono::steady_clock::now();
            rx_reord_ = rx_next_;
        }
        return true;
    }

    void check_timer(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now()) {
        if (!timer_running_) return;

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - timer_start_);
        if (elapsed >= config_.timer_duration) {
            metrics_.timer_flushes++;
            timer_running_ = false;

            // Deliver all stored packets with SN < rx_reord_
            for (auto it = buffer_.begin(); it != buffer_.end() && it->first < rx_reord_; ) {
                sink_(std::move(it->second));
                metrics_.packets_delivered_in_order++;
                it = buffer_.erase(it);
            }

            rx_deliv_ = rx_reord_;
            flush_contiguous();

            if (rx_deliv_ < rx_next_ && !buffer_.empty()) {
                timer_running_ = true;
                timer_start_ = now;
                rx_reord_ = rx_next_;
            }
        }
    }

    void flush_all() {
        for (auto& [sn, pkt] : buffer_) {
            sink_(std::move(pkt));
            metrics_.packets_delivered_in_order++;
        }
        buffer_.clear();
        metrics_.current_buffer_size.store(0);
        timer_running_ = false;
    }

    uint32_t rx_deliv() const { return rx_deliv_; }
    uint32_t rx_next() const { return rx_next_; }
    uint32_t rx_reord() const { return rx_reord_; }
    bool is_timer_running() const { return timer_running_; }
    size_t buffered_count() const { return buffer_.size(); }
    const EngineMetrics& metrics() const { return metrics_; }
};

} // namespace pdcp
