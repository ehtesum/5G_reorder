# 5G NR PDCP Reordering Engine

A C++17 user-space implementation of 3GPP TS 38.323 sequence reordering over UDP sockets.

## Overview

In 5G NR radio access networks, lower-layer retransmissions (HARQ and RLC AM) frequently cause packets to arrive out of order. If passed directly to upper-layer TCP, packet reordering triggers duplicate ACKs and spurious congestion throttling.

The PDCP layer restores strict in-order delivery before forwarding packets to the IP stack. This project implements a standalone PDCP sequence reordering engine using a sliding window buffer and a configurable `t-Reordering` timer to resolve sequence gaps without head-of-line blocking.

## Architecture

* **UDP Ingestion:** Dedicated receiver thread drains incoming UDP datagrams into a thread-safe bounded queue (`std::mutex` + `std::condition_variable`).
* **Sequence Reordering:** Worker thread processes incoming packets according to Sequence Number (SN):
  - In-order (`SN == RX_DELIV`): Passed immediately to the output sink, followed by any consecutively buffered packets.
  - Out-of-order (`SN > RX_DELIV`): Stored in the window buffer, starting `t-Reordering` if not already running.
  - Stale / Duplicate (`SN < RX_DELIV`): Dropped.
* **Timer Expiration:** If a missing packet fails to arrive before `t-Reordering` expires, the buffer advances `RX_DELIV` to the first missing sequence number to keep the pipeline moving.

## Build

```bash
make
```

## Running the Engine

Start the reordering server:
```bash
./build/pdcp_server -p 50500 -t 100
```

Generate out-of-order test traffic in another terminal:
```bash
./build/pdcp_traffic_gen -n 10 --shuffle
```

Flags and parameters:
```bash
./build/pdcp_server --help
./build/pdcp_traffic_gen --help
```
