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

Fill in with real values before recording the first result
(`scripts/capture_env.sh` collects most of this):

- Kernel: `6.18.33.2-microsoft-standard-WSL2` (`uname -a`)
- Distro:
- CPU model:
- Logical CPUs: 24 (`nproc`)
- Cores / threads-per-core / sockets: (`lscpu`)
- NUMA nodes: (`lscpu`)
- Cache sizes L1d/L1i/L2/L3: (`lscpu -C`)
- RAM:
- Compiler: `g++ (Ubuntu 15.2.0-16ubuntu1) 15.2.0`
- CMake / Ninja: `4.2.3` / `1.13.2`
- Build flags: `-std=c++20 -O2 -g -Wall -Wextra -Wpedantic`
- `steady_clock` resolution: (measure in Project 02)
- CPU governor / frequency scaling: (host-controlled under WSL2)

## Known limitations of `wsl2`

- Virtualized kernel; scheduler and interrupt behavior are not the host's.
- No `perf` installed and limited PMU access → stage 2 profiling is constrained.
- No `numactl`; NUMA topology is likely flattened.
- No direct NIC access, no XDP/AF_XDP/DPDK.
- Background Windows activity is invisible noise in every measurement.

Record these as limitations in project READMEs rather than pretending the numbers
are bare-metal numbers.
