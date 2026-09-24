#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <optional>
#include <chrono>
#include <algorithm>

namespace pdcp {

constexpr size_t HEADER_SIZE = 16;
constexpr size_t MAX_PAYLOAD_SIZE = 1472;

namespace Flags {
    constexpr uint8_t NONE          = 0x00;
    constexpr uint8_t END_OF_STREAM = 0x01;
    constexpr uint8_t HEARTBEAT     = 0x02;
    constexpr uint8_t URGENT        = 0x04;
}

struct Packet {
    uint32_t seq_num{0};
    uint64_t timestamp_ns{0};
    uint16_t payload_len{0};
    uint8_t flags{Flags::NONE};
    std::vector<uint8_t> payload;

    Packet() = default;

    Packet(uint32_t seq, std::vector<uint8_t> data, uint8_t flg = Flags::NONE)
        : seq_num(seq),
          timestamp_ns(now_ns()),
          payload_len(static_cast<uint16_t>(data.size())),
          flags(flg),
          payload(std::move(data)) {}

    Packet(uint32_t seq, const std::string& str_data, uint8_t flg = Flags::NONE)
        : seq_num(seq),
          timestamp_ns(now_ns()),
          payload_len(static_cast<uint16_t>(str_data.size())),
          flags(flg),
          payload(str_data.begin(), str_data.end()) {}

    std::string to_string() const {
        return std::string(payload.begin(), payload.end());
    }

    static uint64_t now_ns() {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );
    }

    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> buf(HEADER_SIZE + payload.size());

        buf[0] = static_cast<uint8_t>((seq_num >> 24) & 0xFF);
        buf[1] = static_cast<uint8_t>((seq_num >> 16) & 0xFF);
        buf[2] = static_cast<uint8_t>((seq_num >> 8) & 0xFF);
        buf[3] = static_cast<uint8_t>(seq_num & 0xFF);

        for (int i = 0; i < 8; ++i) {
            buf[4 + i] = static_cast<uint8_t>((timestamp_ns >> (56 - 8 * i)) & 0xFF);
        }

        uint16_t len = static_cast<uint16_t>(payload.size());
        buf[12] = static_cast<uint8_t>((len >> 8) & 0xFF);
        buf[13] = static_cast<uint8_t>(len & 0xFF);
        buf[14] = flags;
        buf[15] = 0x00;

        if (!payload.empty()) {
            std::copy(payload.begin(), payload.end(), buf.begin() + HEADER_SIZE);
        }
        return buf;
    }

    static std::optional<Packet> deserialize(const uint8_t* data, size_t len) {
        if (!data || len < HEADER_SIZE) {
            return std::nullopt;
        }

        Packet pkt;
        pkt.seq_num = (static_cast<uint32_t>(data[0]) << 24) |
                      (static_cast<uint32_t>(data[1]) << 16) |
                      (static_cast<uint32_t>(data[2]) << 8)  |
                      (static_cast<uint32_t>(data[3]));

        pkt.timestamp_ns = 0;
        for (int i = 0; i < 8; ++i) {
            pkt.timestamp_ns = (pkt.timestamp_ns << 8) | static_cast<uint64_t>(data[4 + i]);
        }

        pkt.payload_len = (static_cast<uint16_t>(data[12]) << 8) | static_cast<uint16_t>(data[13]);
        pkt.flags = data[14];

        if (len < HEADER_SIZE + pkt.payload_len) {
            return std::nullopt;
        }

        if (pkt.payload_len > 0) {
            pkt.payload.assign(data + HEADER_SIZE, data + HEADER_SIZE + pkt.payload_len);
        }
        return pkt;
    }
};

} // namespace pdcp
