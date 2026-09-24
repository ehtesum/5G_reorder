#include "packet.hpp"
#include "reorder_buffer.hpp"
#include "thread_safe_queue.hpp"

#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>
#include <cstring>
#include <cstdlib>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

static std::atomic<bool> g_running{true};

static void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        g_running.store(false);
    }
}

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  -p, --port <port>       UDP listen port (default: 50500)\n"
              << "  -w, --window <size>     Sliding window size (default: 512)\n"
              << "  -t, --timer <ms>        Reorder timer in ms (default: 100)\n"
              << "  -h, --help              Show this help message\n";
}

int main(int argc, char* argv[]) {
    int port = 50500;
    uint32_t window = 512;
    int timer_ms = 100;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            port = std::atoi(argv[++i]);
        } else if ((arg == "-w" || arg == "--window") && i + 1 < argc) {
            window = static_cast<uint32_t>(std::atoi(argv[++i]));
        } else if ((arg == "-t" || arg == "--timer") && i + 1 < argc) {
            timer_ms = std::atoi(argv[++i]);
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("[ERROR] socket() creation failed");
        return 1;
    }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("[ERROR] bind() failed");
        close(sock);
        return 1;
    }

    // 100ms receive timeout to allow graceful loop termination
    timeval tv{.tv_sec = 0, .tv_usec = 100000};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    std::cout << "[INFO] PDCP reorder daemon started on 0.0.0.0:" << port
              << " (window=" << window << ", timer=" << timer_ms << "ms)\n"
              << "[INFO] Press Ctrl+C to terminate cleanly.\n";

    pdcp::ThreadSafeQueue<pdcp::Packet> queue(4096);
    pdcp::ReorderConfig cfg;
    cfg.window_size = window;
    cfg.timer_duration = std::chrono::milliseconds(timer_ms);

    pdcp::ReorderBuffer engine(cfg, [](pdcp::Packet pkt) {
        uint64_t now = pdcp::Packet::now_ns();
        uint64_t lat_us = (now > pkt.timestamp_ns) ? (now - pkt.timestamp_ns) / 1000 : 0;
        std::cout << "[DELIVERED] SN=" << pkt.seq_num
                  << " len=" << pkt.payload.size()
                  << " latency=" << lat_us << "us"
                  << " data="" << pkt.to_string() << ""\n";
    });

    // Ingestion Thread
    std::thread rx_thread([&] {
        std::vector<uint8_t> buf(2048);
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);

        while (g_running.load()) {
            ssize_t n = recvfrom(sock, buf.data(), buf.size(), 0,
                                 reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
            if (n > 0) {
                auto pkt_opt = pdcp::Packet::deserialize(buf.data(), static_cast<size_t>(n));
                if (pkt_opt.has_value()) {
                    if (!queue.push(std::move(*pkt_opt))) {
                        std::cerr << "[WARN] Ingestion queue full, dropping datagram\n";
                    }
                } else {
                    std::cerr << "[WARN] Malformed header discarded\n";
                }
            } else if (n < 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                    perror("[WARN] recvfrom error");
                }
            }
        }
    });

    // Processing Thread
    while (g_running.load() || !queue.empty()) {
        pdcp::Packet pkt;
        if (queue.pop_wait(pkt, std::chrono::milliseconds(20))) {
            engine.process_packet(std::move(pkt));
        }
        engine.check_timer();
    }

    std::cout << "\n[INFO] Signal received. Draining remaining buffers...\n";
    queue.shutdown();
    if (rx_thread.joinable()) rx_thread.join();
    close(sock);
    engine.flush_all();

    std::cout << "[INFO] Engine statistics: " << engine.metrics().summary() << "\n"
              << "[INFO] Shutdown complete.\n";
    return 0;
}
