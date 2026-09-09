# Progress

**2 / 72 complete.**

Status: `·` not started · `▶` in progress · `✅` done (README complete + results committed)

A project is ticked only when its README's 11 sections are filled and `results/`
contains real output. See `docs/CONVENTIONS.md` §9.


## 🟢 Stage 1 — C++ + CPU  —  2/10

| | # | Project | Directory | Result headline |
|---|---|---|---|---|
| ✅ | 01 | CPU-Pinned Worker | `01_cpu_pinned_worker` | Pinning fixes placement (0 vs 1 migration, always CPU 4) but shows no measurable latency change on an idle 24-core machine |
| ✅ | 02 | Latency Timer | `02_latency_timer` | Measured `steady_clock` resolution at 10 ns; on the same 20,000-push_back run, a naive mean (~8 ns) completely hid 16 vector reallocations that pushed `max` to 47–94 µs — only the tail percentiles revealed it |
| · | 03 | CPU Migration Detector | `03_cpu_migration` | |
| · | 04 | Cache Latency Lab | `04_cache_latency` | |
| · | 05 | False Sharing Demo | `05_false_sharing` | |
| · | 06 | Atomic Counter Benchmark | `06_atomic_counter` | |
| · | 07 | Memory Ordering Lab | `07_memory_ordering` | |
| · | 08 | SPSC Queue | `08_spsc_queue` | |
| · | 09 | Object Pool | `09_object_pool` | |
| · | 10 | Aligned Data Benchmark | `10_aligned_data` | |


## 🟡 Stage 2 — Linux systems  —  0/10

| | # | Project | Directory | Result headline |
|---|---|---|---|---|
| · | 11 | Thread Scheduler Lab | `11_thread_scheduler` | |
| · | 12 | CPU Isolation Experiment | `12_cpu_isolation` | |
| · | 13 | Context Switch Benchmark | `13_context_switch` | |
| · | 14 | Page Fault Lab | `14_page_fault` | |
| · | 15 | Huge Page Experiment | `15_huge_pages` | |
| · | 16 | NUMA Latency Lab | `16_numa_latency` | |
| · | 17 | Core/NUMA Mapper | `17_topology_mapper` | |
| · | 18 | perf Profiling Lab | `18_perf_profiling` | |
| · | 19 | CPU Frequency Experiment | `19_cpu_frequency` | |
| · | 20 | Low-Latency Worker | `20_low_latency_worker` | |


## 🟠 Stage 3 — Networking  —  0/10

| | # | Project | Directory | Result headline |
|---|---|---|---|---|
| · | 21 | TCP Echo Server | `21_tcp_echo_server` | |
| · | 22 | UDP Echo Server | `22_udp_echo_server` | |
| · | 23 | UDP Latency Tester | `23_udp_latency_tester` | |
| · | 24 | Nonblocking UDP Engine | `24_nonblocking_udp` | |
| · | 25 | Packet Size Benchmark | `25_packet_size_bench` | |
| · | 26 | Multicast Sender | `26_multicast_sender` | |
| · | 27 | Multicast Receiver | `27_multicast_receiver` | |
| · | 28 | Sequence Number Monitor | `28_sequence_monitor` | |
| · | 29 | Packet Replay Tool | `29_packet_replay` | |
| · | 30 | Timestamping Lab | `30_timestamping_lab` | |


## 🔴 Stage 4 — NIC + Linux low latency  —  0/10

| | # | Project | Directory | Result headline |
|---|---|---|---|---|
| · | 31 | NIC Inspection Tool | `31_nic_inspection` | |
| · | 32 | NIC Queue Inspection | `32_nic_queues` | |
| · | 33 | RSS Experiment | `33_rss_experiment` | |
| · | 34 | IRQ Affinity Experiment | `34_irq_affinity` | |
| · | 35 | CPU ↔ NIC Mapping | `35_cpu_nic_mapping` | |
| · | 36 | Busy-Poll Receiver | `36_busy_poll_receiver` | |
| · | 37 | Socket Buffer Benchmark | `37_socket_buffer_bench` | |
| · | 38 | Zero-Allocation UDP Receiver | `38_zero_alloc_udp` | |
| · | 39 | End-to-End Packet Benchmark | `39_e2e_packet_bench` | |
| · | 40 | Low-Jitter Receiver | `40_low_jitter_receiver` | |


## 🔵 Stage 5 — Kernel bypass  —  0/13

| | # | Project | Directory | Result headline |
|---|---|---|---|---|
| · | 41 | XDP Packet Counter | `41_xdp_counter` | |
| · | 42 | XDP Packet Filter | `42_xdp_filter` | |
| · | 43 | AF_XDP Receiver | `43_af_xdp_receiver` | |
| · | 44 | AF_XDP Latency Benchmark | `44_af_xdp_latency` | |
| · | 45 | AF_XDP Multi-Queue | `45_af_xdp_multiqueue` | |
| · | 46 | DPDK Hello / Port Setup | `46_dpdk_port_setup` | |
| · | 47 | DPDK RX Loop | `47_dpdk_rx_loop` | |
| · | 48 | DPDK TX Loop | `48_dpdk_tx_loop` | |
| · | 49 | DPDK Mempool Lab | `49_dpdk_mempool` | |
| · | 50 | DPDK Ring Lab | `50_dpdk_ring` | |
| · | 51 | DPDK Multi-Core Receiver | `51_dpdk_multicore_rx` | |
| · | 52 | DPDK Queue/Core Affinity | `52_dpdk_queue_affinity` | |
| · | 53 | DPDK Packet Benchmark | `53_dpdk_packet_bench` | |


## 🟣 Stage 6 — Trading components  —  0/13

| | # | Project | Directory | Result headline |
|---|---|---|---|---|
| · | 54 | Binary Protocol Lab | `54_binary_protocol` | |
| · | 55 | Market Data Generator | `55_market_data_gen` | |
| · | 56 | Feed Handler | `56_feed_handler` | |
| · | 57 | Fast Parser | `57_fast_parser` | |
| · | 58 | Order Book v1 | `58_order_book_v1` | |
| · | 59 | Order Book v2 | `59_order_book_v2` | |
| · | 60 | Order Book Benchmark | `60_order_book_bench` | |
| · | 61 | Trade/Event Engine | `61_event_engine` | |
| · | 62 | Strategy Engine | `62_strategy_engine` | |
| · | 63 | Risk Engine | `63_risk_engine` | |
| · | 64 | OMS | `64_oms` | |
| · | 65 | Exchange Simulator | `65_exchange_simulator` | |
| · | 66 | Order Gateway | `66_order_gateway` | |


## 🟣 Stage 7 — Put it together  —  0/6

| | # | Project | Directory | Result headline |
|---|---|---|---|---|
| · | 67 | Low-Latency Trading Pipeline | `67_trading_pipeline` | |
| · | 68 | Lock-Free Trading Pipeline | `68_lockfree_pipeline` | |
| · | 69 | NUMA-Aware Trading Engine | `69_numa_aware_engine` | |
| · | 70 | DPDK Trading Engine | `70_dpdk_trading_engine` | |
| · | 71 | Full Latency Measurement | `71_full_latency_measure` | |
| · | 72 | Stress Test / Final System | `72_final_trading_system` | |


---

## Log

| Date | Project | Note |
|---|---|---|
| 2026-09-07 | — | Repository architecture created; no project started. |
| 2026-09-08 | 01 | CPU-Pinned Worker complete: unpinned vs. pinned (CPU 4), 5 reps each. Pinning made placement deterministic (0 vs 1 migration) but no measurable latency change on an idle 24-core Ryzen 9 7900X. |
| 2026-09-09 | 02 | Latency Timer complete: measured `steady_clock` resolution (10 ns) and built a nearest-rank percentile calculator. On identical 20,000-push_back runs, a naive aggregate mean (~8 ns) could not see 16 real vector reallocations that drove `max` to 47–94 µs; even `p99.9` only partially captured it (arithmetic reason documented in README §8), demonstrating why distributions, not averages, are the standard from here on. |
