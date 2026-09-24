#include "packet.hpp"

#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <algorithm>
#include <random>
#include <thread>
#include <chrono>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  -a, --addr <ip>         Target IP address (default: 127.0.0.1)\n"
              << "  -p, --port <port>       Target UDP port (default: 50500)\n"
              << "  -n, --count <num>       Number of packets to send (default: 10)\n"
              << "  -s, --shuffle           Inject packets out-of-order\n"
              << "  -d, --delay <ms>        Delay between packets in ms (default: 20)\n"
              << "  -h, --help              Show this help\n";
}

int main(int argc, char* argv[]) {
    std::string ip = "127.0.0.1";
    int port = 50500;
    int count = 10;
    int delay_ms = 20;
    bool shuffle = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-a" || arg == "--addr") && i + 1 < argc) {
            ip = argv[++i];
        } else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            port = std::atoi(argv[++i]);
        } else if ((arg == "-n" || arg == "--count") && i + 1 < argc) {
            count = std::atoi(argv[++i]);
        } else if ((arg == "-d" || arg == "--delay") && i + 1 < argc) {
            delay_ms = std::atoi(argv[++i]);
        } else if (arg == "-s" || arg == "--shuffle") {
            shuffle = true;
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("[ERROR] socket() failed");
        return 1;
    }

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(static_cast<uint16_t>(port));
    dest.sin_addr.s_addr = inet_addr(ip.c_str());

    std::vector<uint32_t> seqs;
    seqs.reserve(count);
    for (int i = 0; i < count; ++i) {
        seqs.push_back(static_cast<uint32_t>(i));
    }

    if (shuffle && count > 1) {
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(seqs.begin(), seqs.end(), g);
        std::cout << "[INFO] Shuffled sequence enabled.\n";
    }

    std::cout << "[INFO] Sending " << count << " UDP datagrams to " << ip << ":" << port << "...\n";

    for (uint32_t seq : seqs) {
        std::string msg = "payload_seq_" + std::to_string(seq);
        pdcp::Packet pkt(seq, msg);
        std::vector<uint8_t> wire = pkt.serialize();

        ssize_t sent = sendto(sock, wire.data(), wire.size(), 0,
                              reinterpret_cast<sockaddr*>(&dest), sizeof(dest));
        if (sent < 0) {
            perror("[ERROR] sendto failed");
            break;
        }
        std::cout << "  -> Transmitted SN=" << seq << " (" << sent << " bytes)\n";

        if (delay_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        }
    }

    close(sock);
    std::cout << "[INFO] Transmission complete.\n";
    return 0;
}
