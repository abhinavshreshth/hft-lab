# Roadmap — 72 projects

Source: `docs/reference/roadmap_rough_note.txt`.

Directory names are fixed here so that cross-references written today stay valid.
A directory is created **only when its project is started**.

---

## 🟢 Stage 1 — C++ + CPU (01–10)
Foundation: threads, timing, cache, atomics, lock-free basics.

| # | Directory | Project | Concept taught |
|---|---|---|---|
| 01 | `01_cpu_pinned_worker` | CPU-Pinned Worker | `std::thread`, CPU affinity |
| 02 | `02_latency_timer` | Latency Timer | `steady_clock`, distributions, p50/p99 |
| 03 | `03_cpu_migration` | CPU Migration Detector | scheduler behavior, `sched_getcpu()` |
| 04 | `04_cache_latency` | Cache Latency Lab | cache vs RAM, access patterns |
| 05 | `05_false_sharing` | False Sharing Demo | cache lines, contention |
| 06 | `06_atomic_counter` | Atomic Counter Benchmark | atomics vs mutex |
| 07 | `07_memory_ordering` | Memory Ordering Lab | acquire/release/relaxed |
| 08 | `08_spsc_queue` | SPSC Queue | lock-free programming |
| 09 | `09_object_pool` | Object Pool | allocation overhead |
| 10 | `10_aligned_data` | Aligned Data Benchmark | alignment, cache-line layout |

## 🟡 Stage 2 — Linux systems (11–20)
The OS underneath the code: scheduling, memory, NUMA, profiling.

| # | Directory | Project | Concept taught |
|---|---|---|---|
| 11 | `11_thread_scheduler` | Thread Scheduler Lab | scheduling / preemption |
| 12 | `12_cpu_isolation` | CPU Isolation Experiment | dedicated cores |
| 13 | `13_context_switch` | Context Switch Benchmark | context-switch cost |
| 14 | `14_page_fault` | Page Fault Lab | virtual memory |
| 15 | `15_huge_pages` | Huge Page Experiment | TLB / page behavior |
| 16 | `16_numa_latency` | NUMA Latency Lab | local vs remote memory |
| 17 | `17_topology_mapper` | Core/NUMA Mapper | `lscpu`, `numactl`, topology |
| 18 | `18_perf_profiling` | perf Profiling Lab | cycles, cache misses |
| 19 | `19_cpu_frequency` | CPU Frequency Experiment | frequency / turbo effects |
| 20 | `20_low_latency_worker` | Low-Latency Worker | **integration:** affinity + memory + profiling |

## 🟠 Stage 3 — Networking (21–30)
Sockets, UDP, multicast — the shape of a market-data feed.

| # | Directory | Project | Concept taught |
|---|---|---|---|
| 21 | `21_tcp_echo_server` | TCP Echo Server | TCP fundamentals |
| 22 | `22_udp_echo_server` | UDP Echo Server | UDP |
| 23 | `23_udp_latency_tester` | UDP Latency Tester | network latency |
| 24 | `24_nonblocking_udp` | Nonblocking UDP Engine | polling / event behavior |
| 25 | `25_packet_size_bench` | Packet Size Benchmark | packet overhead |
| 26 | `26_multicast_sender` | Multicast Sender | multicast |
| 27 | `27_multicast_receiver` | Multicast Receiver | market-data style feed |
| 28 | `28_sequence_monitor` | Sequence Number Monitor | packet loss / ordering |
| 29 | `29_packet_replay` | Packet Replay Tool | deterministic testing |
| 30 | `30_timestamping_lab` | Timestamping Lab | packet timing |

## 🔴 Stage 4 — NIC + Linux low latency (31–40)
Hardware datapath: queues, RSS, IRQs, busy-polling.

| # | Directory | Project | Concept taught |
|---|---|---|---|
| 31 | `31_nic_inspection` | NIC Inspection Tool | NIC / driver capabilities |
| 32 | `32_nic_queues` | NIC Queue Inspection | RX/TX queues |
| 33 | `33_rss_experiment` | RSS Experiment | packet → CPU distribution |
| 34 | `34_irq_affinity` | IRQ Affinity Experiment | NIC interrupts |
| 35 | `35_cpu_nic_mapping` | CPU ↔ NIC Mapping | dedicated datapath |
| 36 | `36_busy_poll_receiver` | Busy-Poll Receiver | polling vs interrupts |
| 37 | `37_socket_buffer_bench` | Socket Buffer Benchmark | kernel networking tuning |
| 38 | `38_zero_alloc_udp` | Zero-Allocation UDP Receiver | allocation-free hot path |
| 39 | `39_e2e_packet_bench` | End-to-End Packet Benchmark | complete latency path |
| 40 | `40_low_jitter_receiver` | Low-Jitter Receiver | **integration:** all of stage 4 |

## 🔵 Stage 5 — Kernel bypass (41–53)
XDP → AF_XDP → DPDK.

| # | Directory | Project | Concept taught |
|---|---|---|---|
| 41 | `41_xdp_counter` | XDP Packet Counter | XDP basics |
| 42 | `42_xdp_filter` | XDP Packet Filter | kernel-side packet processing |
| 43 | `43_af_xdp_receiver` | AF_XDP Receiver | userspace packet reception |
| 44 | `44_af_xdp_latency` | AF_XDP Latency Benchmark | compare against UDP |
| 45 | `45_af_xdp_multiqueue` | AF_XDP Multi-Queue | queue / core mapping |
| 46 | `46_dpdk_port_setup` | DPDK Hello / Port Setup | DPDK architecture |
| 47 | `47_dpdk_rx_loop` | DPDK RX Loop | poll-mode driver |
| 48 | `48_dpdk_tx_loop` | DPDK TX Loop | packet transmission |
| 49 | `49_dpdk_mempool` | DPDK Mempool Lab | packet buffers |
| 50 | `50_dpdk_ring` | DPDK Ring Lab | inter-thread communication |
| 51 | `51_dpdk_multicore_rx` | DPDK Multi-Core Receiver | scaling |
| 52 | `52_dpdk_queue_affinity` | DPDK Queue/Core Affinity | NIC/CPU optimization |
| 53 | `53_dpdk_packet_bench` | DPDK Packet Benchmark | pps + latency |

## 🟣 Stage 6 — Trading components (54–66)
The projects stop being generic networking experiments.

| # | Directory | Project | What you build |
|---|---|---|---|
| 54 | `54_binary_protocol` | Binary Protocol Lab | fixed binary message format |
| 55 | `55_market_data_gen` | Market Data Generator | exchange simulator |
| 56 | `56_feed_handler` | Feed Handler | receive + decode messages |
| 57 | `57_fast_parser` | Fast Parser | zero/minimal-allocation parser |
| 58 | `58_order_book_v1` | Order Book v1 | bids / asks |
| 59 | `59_order_book_v2` | Order Book v2 | incremental updates |
| 60 | `60_order_book_bench` | Order Book Benchmark | throughput / latency |
| 61 | `61_event_engine` | Trade/Event Engine | process book events |
| 62 | `62_strategy_engine` | Strategy Engine | generate decisions |
| 63 | `63_risk_engine` | Risk Engine | position / order limits |
| 64 | `64_oms` | OMS | order lifecycle |
| 65 | `65_exchange_simulator` | Exchange Simulator | accept / reject / fill |
| 66 | `66_order_gateway` | Order Gateway | outbound order path |

## 🟣 Stage 7 — Put it together (67–72)

| # | Directory | Project | Goal |
|---|---|---|---|
| 67 | `67_trading_pipeline` | Low-Latency Trading Pipeline | wire 56→66 end to end |
| 68 | `68_lockfree_pipeline` | Lock-Free Trading Pipeline | SPSC/MPSC queues, atomic state, cache-aware layout |
| 69 | `69_numa_aware_engine` | NUMA-Aware Trading Engine | keep NIC/threads/data on one node |
| 70 | `70_dpdk_trading_engine` | DPDK Trading Engine | swap UDP for DPDK RX/TX, benchmark the delta |
| 71 | `71_full_latency_measure` | Full Latency Measurement | NIC RX → … → NIC TX; min/p50/p95/p99/p99.9/max |
| 72 | `72_final_trading_system` | Stress Test | 1M/2M/5M msg/s: throughput, latency, jitter, CPU, loss |

### Stage 7 pipeline

```
Market Data → Feed Handler → Parser → Order Book → Strategy → Risk → OMS → Order Gateway
```

---

## Reuse map (what feeds the final system)

These are the projects whose code is expected to survive into stage 7, and therefore
the ones whose interfaces deserve extra care when written:

- `02` latency timer      → measurement everywhere, and project 71
- `08` SPSC queue         → project 68 pipeline stages
- `09` object pool        → projects 57, 64
- `10` aligned data       → projects 58/59 order book layout
- `20` low-latency worker → the thread/core model of project 69
- `38` zero-alloc UDP     → project 56 feed handler
- `54` binary protocol    → projects 55, 56, 57, 65

Everything else is allowed to be a throwaway experiment. That is fine — the
measurement and the write-up are the deliverable, not the code.
