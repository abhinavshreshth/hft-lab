# Environment

Every result in this repo names the environment that produced it. This file is
the canonical description; project READMEs record only the deltas.

## Environments

| ID | Description | Suitable for |
|---|---|---|
| `wsl2` | Windows + WSL2 Ubuntu | Stages 1–3 (CPU, memory, concurrency, sockets) |
| `native` | *(to be added)* bare-metal Linux | Stages 4–7 (NIC, IRQs, XDP/AF_XDP, DPDK) |

Stages 4+ need control over IRQ affinity, core isolation, huge pages and the NIC
itself; WSL2 cannot provide these. Plan for native Linux before Project 31.

## `wsl2` — current machine

Captured 2026-09-08 via `scripts/capture_env.sh`:

- Kernel: `6.18.33.2-microsoft-standard-WSL2` (`uname -a`)
- Distro: Ubuntu 26.04 LTS
- CPU model: AMD Ryzen 9 7900X 12-Core Processor
- Logical CPUs: 24 (`nproc`)
- Cores / threads-per-core / sockets: 12 / 2 (SMT) / 1
- NUMA nodes: 1 (all 24 CPUs on node0 — expected for a single-socket desktop part)
- Cache sizes: L1d 32K, L1i 32K per core (384K/384K total) · L2 1M per core (12M total) · L3 32M shared
- RAM: 30 GiB total (`free -h`)
- Compiler: `g++ (Ubuntu 15.2.0-16ubuntu1) 15.2.0`
- CMake / Ninja: `4.2.3` / `1.13.2`
- Build flags: `-std=c++20 -O2 -g -Wall -Wextra -Wpedantic`
- `steady_clock` resolution: 10 ns (min observed tick, see 02_latency_timer/README.md §7)
- CPU governor / frequency scaling: not exposed under WSL2 (`scaling_governor` unreadable — host-controlled)
- Huge pages: `Hugepagesize: 2048 kB`, `HugePages_Total: 0` (none reserved)

## Known limitations of `wsl2`

- Virtualized kernel; scheduler and interrupt behavior are not the host's.
- No `perf` installed and limited PMU access → stage 2 profiling is constrained.
- No `numactl`; NUMA topology is likely flattened.
- No direct NIC access, no XDP/AF_XDP/DPDK.
- Background Windows activity is invisible noise in every measurement.

Record these as limitations in project READMEs rather than pretending the numbers
are bare-metal numbers.
