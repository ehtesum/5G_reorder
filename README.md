# 5G NR PDCP Reordering Engine

A lightweight C++17 user-space implementation of 3GPP TS 38.323 sequence reordering over UDP.

## Motivation

In 5G NR radio access networks, lower-layer retransmissions (HARQ and RLC AM) cause packets to arrive out of order. Upper-layer protocols like TCP interpret packet reordering as congestion, causing unnecessary throughput throttling. 

The PDCP (Packet Data Convergence Protocol) layer sits above RLC and restores strict in-order delivery before handing packets to the IP layer, using a sliding window and a reordering timer (`t-Reordering`) to prevent head-of-line blocking when packets are dropped over the radio interface.

## How It Works

1. **UDP Ingestion**: A receiver thread continuously drains UDP datagrams and pushes parsed packets into a bounded, thread-safe queue (`std::mutex` + `std::condition_variable`).
2. **Sequence Reordering**: The worker thread evaluates each packet's Sequence Number (SN):
   - **In-order (`SN == RX_DELIV`)**: Delivered immediately to the sink; triggers a check for any consecutively buffered packets.
   - **Out-of-order (`SN > RX_DELIV`)**: Stored in the window buffer, and the `t-Reordering` timer is started if not already running.
   - **Duplicate / Stale (`SN < RX_DELIV`)**: Dropped.
3. **Gap Resolution**: If the missing packet does not arrive before `t-Reordering` expires, the buffer advances `RX_DELIV` to the next expected sequence to avoid stalling the pipeline.

## Build

```bash
make
```

## Quick Start

Start the server:
```bash
./build/pdcp_server -p 50500 -t 100
```

In another terminal, send out-of-order test packets:
```bash
./build/pdcp_traffic_gen -n 10 --shuffle
```

## CLI Options

Pass `--help` to either binary for configuration flags:
```bash
./build/pdcp_server --help
./build/pdcp_traffic_gen --help
```
