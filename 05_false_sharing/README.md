# Project 05 — False Sharing Demo

Stage 1 · Concept: cache lines, contention (false sharing)

## 1. Objective

Project 04 measured what one thread pays to reach memory. This project puts
a *second* writer on the machine and asks: if two threads never touch each
other's data — no shared variables, no locks, no atomics — can they still
slow each other down, purely because their private variables happen to sit
in the same 64-byte cache line? If so, by how much, how does it scale with
the number of writers, exactly how far apart do the variables have to be
for it to stop, and does it depend on which cores the threads run on?

## 2. Environment

Base: `docs/ENVIRONMENT.md` (`wsl2`). No deltas — AMD Ryzen 9 7900X, 12
cores / 24 logical CPUs, 64-byte cache lines, machine otherwise idle during
all runs.

One environment fact matters here more than in Projects 01–04: WSL2's sysfs
reports a single 32 MiB L3 shared by all 24 CPUs
(`cache/index3/shared_cpu_list = 0-23`), and SMT pairs `(0,1) (2,3) …`. The
guest can pin a thread to a *virtual* CPU, but it cannot see or control
which host core that vCPU is running on. Section 8 shows why that matters.

## 3. What I tested

Seven thin experiment executables on top of a small library, `src/lab/`
(built as the `05_false_sharing_lab` static library):

```
05_false_sharing/src/
├── lab/                       the apparatus (not experiments)
│   ├── cache_line.hpp         kCacheLineSize, kMaxThreads, line_of()
│   ├── counter_layout.hpp     WHERE counters live — packed / padded / offset
│   ├── cpu_placement.*        core ids, one_cpu_per_core(), pinning, CPU args
│   ├── race_runner.*          HOW threads race — pin, barrier, timed loop
│   ├── trials.*               warmup + trials, same-run comparison of two layouts
│   ├── sanity.*               the checks Experiment 1 runs on the apparatus
│   └── report.*               ALL console output
├── exp1_layout_sanity.cpp     Experiment 1 — validate the apparatus itself
├── exp2_packed.cpp            Experiment 2 — baseline: 2 threads, counters in ONE line
├── exp3_padded.cpp            Experiment 3 — modified: 2 threads, one line per counter
├── exp4_compare.cpp           Experiment 4 — packed vs padded in ONE run
├── exp5_thread_scaling.cpp    Experiment 5 — 1..8 writers, packed vs padded
├── exp6_offset_sweep.cpp      Experiment 6 — distance between the two counters, 8..512 B
└── exp7_placement_sweep.cpp   Experiment 7 — partner thread on every other logical CPU
```

Each `expN_*.cpp` holds only the experiment itself: which layout, which
CPUs, what gets compared. It hands its results to `report::`, the only
place that prints anything. The printed format is therefore owned by one
module, and `results/` captures stay comparable when an experiment changes.

The apparatus splits the same way Project 04's `cache_lab.hpp` split
`ChainBuilder` from `PointerChaser`:

- **`CounterLayout`** — where each thread's counter lives (the
  experimental variable). Three implementations:
  - `PackedLayout` — 8 `uint64_t` in one 64-byte-aligned block, i.e. what
    `std::uint64_t counters[8];` gives you by default. Every counter is in
    the same line.
  - `PaddedLayout` — an array of `struct alignas(64) PaddedCounter { uint64_t value; }`,
    the standard fix: `alignas(64)` pads `sizeof` to 64, so each counter
    owns a whole line.
  - `OffsetLayout(k)` — two counters `k` bytes apart in a line-aligned
    buffer, for the offset sweep.
- **`RaceRunner`** — the fixed apparatus. It starts one `std::thread` per
  requested CPU, and each thread pins itself (`pthread_setaffinity_np`) and
  checks `sched_getcpu()`. A start barrier releases all threads at once. Each
  thread then times its own loop of `*c = *c + 1` on its own counter. It
  never branches on which layout it was handed.

The counter is accessed through `volatile std::uint64_t*` so that `-O2`
must emit a real load and store on every increment (checked in the
disassembly: `mov (%rbx),%rax; add $1,%rax; mov %rax,(%rbx)`). It is
deliberately *not* `std::atomic`: every counter has exactly one writer, so
there is no data race to protect against. The only thing the threads
share is the cache line. Atomics are Project 06.

`diff src/exp2_packed.cpp src/exp3_padded.cpp` shows the one code change
that matters: `PackedLayout layout;` → `PaddedLayout layout;`. Everything
else in the diff is comments and labels.

```bash
cmake --build build
./build/05_false_sharing/05_exp1_layout_sanity
./build/05_false_sharing/05_exp2_packed          [cpuA cpuB]
./build/05_false_sharing/05_exp3_padded          [cpuA cpuB]
./build/05_false_sharing/05_exp4_compare         [cpuA cpuB]
./build/05_false_sharing/05_exp5_thread_scaling
./build/05_false_sharing/05_exp6_offset_sweep    [cpuA cpuB]
./build/05_false_sharing/05_exp7_placement_sweep [anchor_cpu]
```

Reproducing the measurements (`benchmark/run.sh <binary> <reps> <label>`):

```bash
benchmark/run.sh 05_exp1_layout_sanity   5 sanity
benchmark/run.sh 05_exp2_packed          5 packed
benchmark/run.sh 05_exp3_padded          5 padded
benchmark/run.sh 05_exp4_compare         5 compare
benchmark/run.sh 05_exp5_thread_scaling  5 thread_scaling
benchmark/run.sh 05_exp6_offset_sweep    5 offset_sweep
benchmark/run.sh 05_exp7_placement_sweep 5 placement_sweep
```

## 4. Baseline configuration

`05_exp2_packed` has two threads pinned to CPU 2 (core 1) and CPU 4
(core 2), two distinct physical cores away from CPU 0. Each thread does
50,000,000 increments of its own counter, and the two counters are adjacent
`uint64_t`s in one cache line. This is the right baseline because it is the
layout you get without thinking about it: per-thread stats in a plain
array, or two hot fields next to each other in one struct.

## 5. Modified configuration

`05_exp3_padded` uses the identical harness, CPUs and loop. The only
change is that each counter is an `alignas(64)` `PaddedCounter`, so the two
counters are in different lines.

Hypothesis (written into the experiment sources before the recorded runs;
a short calibration run to choose iteration counts came first and already
hinted at the ~5x magnitude, so the magnitude is not claimed as a
prediction):

1. To write a line, a core must hold it exclusively (MOESI "Modified").
   With both counters in one line, each core's stores keep invalidating the
   other core's copy, so the line must be transferred back and forth.
   Packed should therefore be clearly slower than padded, even though the
   threads share no data.
2. With one thread there is nothing to ping-pong with, so packed and padded
   should cost the same (Experiment 1).
3. More writers on one line → more cores queueing for it → per-thread cost
   should grow with writer count, while padded stays flat (Experiment 5).
4. The coherence unit is the 64-byte line, so the penalty should be the
   same at every offset below 64 and vanish at 64 and above — one cliff. A
   second step at 128 would mean the hardware couples adjacent lines
   (Experiment 6).
5. On an SMT sibling (same physical core, same L1d) the line never has to
   leave the core, so most of the penalty should disappear. On the other
   CCD the transfer crosses the Infinity Fabric, so the penalty should be
   larger there (Experiment 7).

Experiment 4 removes the separate-process confound from 2 vs 3. Both
layouts race on the same CPUs in one execution, and the order alternates
every trial.

## 6. Measurements

- **What is timed:** each thread times its own increment loop with
  `steady_clock`, from just after the start barrier releases it to the end
  of its last increment. Thread creation, pinning and the barrier are
  outside the timed region. The reported number is **slowest-thread
  ns/increment**, because the race is over only when the last thread
  finishes.
- **No per-increment timing:** an uncontended increment is ~0.2 ns, far
  below `steady_clock`'s 10 ns resolution (Project 02). So the loop is
  timed as a whole and divided by the iteration count.
- **Iterations:** 50,000,000 per thread (Experiments 1–4) or 20,000,000
  (sweeps 5–7). The shortest timed loop is therefore ≥ 4 ms, which is ≫
  the clock resolution.
- **Warmup:** one full race per configuration, run and discarded before
  the timed trials (`kWarmupRaces = 1`). It page-faults the counters in and
  warms the caches and clocks.
- **Trials and repetitions:** 11 timed trials per configuration inside one
  process, reported as min / p50 / max across trials. With 11 samples,
  p95/p99/p99.9 would all be the max, so they are not printed. Each
  executable was run **5 times** (`benchmark/run.sh … 5`). The tables below
  give the **mean of the per-process p50s**, with the range across the 5
  processes where it matters.
- **Correctness checks on every race, warmup included:** every counter ends
  at exactly `iterations`, and every thread ran on the CPU it asked for.
  All 30 process-level `checks:` lines read `ok`, and all 5 sanity runs
  printed `ALL CHECKS PASSED`.
- **Outliers:** none discarded.

## 7. Results

### Experiment 1 — sanity (5/5 runs passed every check)

| Check | Result |
|---|---|
| `kCacheLineSize` / `hardware_destructive_interference_size` / sysfs `coherency_line_size` | 64 / 64 / 64 |
| Packed: 8 counters in one line · Padded: 8 counters in 8 lines · Offset: same line iff offset < 64 | all PASS |
| 1 thread, packed/padded p50 ratio | 0.983 – 1.016 |

### Experiments 2–4 — packed vs padded, 2 threads (CPUs 2 and 4)

| | Packed p50 (ns/incr) | Padded p50 (ns/incr) | Packed / padded |
|---|---|---|---|
| Separate runs (exp2 / exp3), per process | 0.870, 1.299, 0.848, 1.085, 0.835 | 0.200, 0.200, 0.200, 0.196, 0.196 | — |
| Separate runs, mean | **0.987** | **0.198** | **5.0x** |
| Same run (exp4), per process | 0.881, 0.865, 1.294, 1.256, 1.308 | 0.196 – 0.200 | 4.42, 4.42, 6.58, 6.42, 6.67 |
| Same run, mean | **1.121** | **0.197** | **5.7x** |

### Experiment 5 — writers on one line (one per physical core: CPUs 2,4,…,16)

Mean of p50 over 5 runs. "Aggregate" = threads ÷ ns-per-increment, i.e.
total increments completed per ns (= billions of increments per second)
across all threads.

| Threads | Padded ns/incr | Packed ns/incr | Packed / padded | Aggregate padded | Aggregate packed |
|---|---|---|---|---|---|
| 1 | 0.197 | 0.201 | 1.0x | 5.08 | 4.99 |
| 2 | 0.201 | 1.046 | 5.2x | 9.96 | **1.91** |
| 3 | 0.200 | 1.474 | 7.4x | 14.99 | 2.04 |
| 4 | 0.201 | 2.040 | 10.1x | 19.88 | 1.96 |
| 5 | 0.206 | 2.568 | 12.5x | 24.32 | 1.95 |
| 6 | 0.207 | 3.260 | 15.7x | 28.96 | 1.84 |
| 7 | 0.208 | 4.132 | 19.9x | 33.65 | 1.69 |
| 8 | 0.222 | 5.398 | 24.3x | 35.97 | 1.48 |

### Experiment 6 — distance between the two counters (CPUs 2 and 4)

| Offset | Same line? | p50 ns/incr (mean of 5) | Range across 5 runs |
|---|---|---|---|
| 8 – 56 B (7 offsets) | yes | 1.016 – 1.113 | 0.821 – 1.340 |
| 64 B | no | 0.198 | 0.195 – 0.202 |
| 72 / 96 / 120 B | no | 0.200 / 0.200 / 0.201 | 0.196 – 0.209 |
| **128 B** | no | **0.198** | 0.197 – 0.199 |
| 136 / 192 / 256 / 512 B | no | 0.198 / 0.198 / 0.201 / 0.202 | 0.195 – 0.208 |

### Experiment 7 — partner thread on every other logical CPU (anchor CPU 2)

| Partner | Relation (per guest sysfs) | Padded p50 | Packed p50 | Ratio |
|---|---|---|---|---|
| CPU 3 | SMT sibling of CPU 2 | 0.200 | **0.899** | 4.49x |
| 22 other CPUs (0, 1, 4–23) | other core | 0.198 – 0.203 | **0.872 – 0.912** (mean 0.897) | 4.35 – 4.60x |

(Per-partner means of 5 runs. No partner, and no group of partners such as
CPUs 12–23, stood out.)

Raw output: `results/2026-09-27_sanity.txt`, `results/2026-09-27_packed.txt`,
`results/2026-09-27_padded.txt`, `results/2026-09-27_compare.txt`,
`results/2026-09-27_thread_scaling.txt`, `results/2026-09-27_offset_sweep.txt`,
`results/2026-09-27_placement_sweep.txt`

## 8. Why the results happened

**Hypotheses 1–4 were confirmed. Hypothesis 5 (placement) was not.**

**False sharing is real, and it comes from the layout alone (H1, H2).**
With one thread, packed and padded are identical (ratio 0.98–1.02), so the
layout itself costs nothing. Add a second writer on another core and the
packed layout becomes ~5x slower (5.0x across separate runs, 5.7x within
one run). The threads never read or write each other's counter; the
correctness check shows every counter ended exactly where its owner left
it. The only shared thing is the 64-byte line, and that is enough. Under
the coherence protocol, a core may only write a line it holds exclusively.
Each store from core A invalidates core B's copy, B's next store has to
fetch the line back and invalidate A, and so on: the *line* ping-pongs even
though no *data* is shared.

**Why "only" ~5x, when a line transfer costs more than an increment.**
Padded runs at ~0.2 ns per increment, about one per clock cycle, because
the load-add-store chain is fed by store-to-load forwarding and never
waits on L1. Packed runs at ~1 ns per increment. That is far *less* than
one core-to-core line transfer per increment would cost: Project 04
measured ~6–11 ns for a plain L3 hit, and a transfer out of another core's
private cache is no cheaper. So each time a core gets the line it must
complete *many* increments, not one. The likely mechanism is the store
buffer. Each thread's load is served from its own pending store to its own
counter, so the thread keeps executing while the line is away. Its stores
queue in the store buffer and drain in a burst once the line arrives, and
the thread stalls only when the store buffer fills. This is an inference:
WSL2 exposes no PMU counters (`docs/ENVIRONMENT.md`), so it could not be
confirmed with `perf`. Snoop-triggered memory-ordering pipeline flushes are
another candidate contributor that could not be separated from it here. It
does predict something testable in Project 06: a locked read-modify-write
(`fetch_add`) must own the line *at each operation* and cannot hide behind
the store buffer, so its false-sharing penalty should be much larger.

**More writers: the line is the bottleneck, not the cores (H3).** Packed
per-thread cost climbs roughly linearly, +0.4 to +1.3 ns per extra writer,
growing toward the top (1.05 ns at 2 threads, 5.40 ns at 8), while padded stays at ~0.2 ns. The
aggregate column is the real lesson. With false sharing, total throughput
stays pinned at **~1.5–2.0 billion increments/s no matter how many threads
are added**, which is *lower than one thread alone* (4.99). The single
line can only be owned by one core at a time, so the whole machine's work
is capped by the rate at which that line can move between cores. Two
threads on a falsely-shared line finished 2.6x *less* total work than one
thread. Padded scales almost perfectly (5.08 → 35.97, 7.1x at 8 threads).
Padded does slow slightly at 8 threads (0.222, with one run at 0.284).
That is plausibly lower all-core boost clocks with more cores busy, but
CPU frequency is host-controlled and invisible under WSL2, so this is
unconfirmed.

**The cliff is exactly at 64 bytes, and there is nothing at 128 (H4).**
Every offset from 8 to 56 B costs the same (~1.0–1.1 ns). The moment the
second counter crosses into the next line (64 B), the cost drops to the
padded ~0.2 ns and stays flat all the way to 512 B. 128 B is no different
from 64 B. So on this CPU the coherence unit is the 64-byte line and
nothing couples adjacent lines for writes. `alignas(64)` is sufficient.
Some codebases pad to 128 B to cover CPUs whose prefetchers fetch
adjacent-line pairs. On this machine that would cost memory and buy
nothing measurable.

**Placement made no difference, and that is a WSL2 finding, not a
hardware one (H5 refuted).** The prediction was that an SMT sibling (CPU 3,
sharing CPU 2's L1d) would mostly lose the penalty and a partner on the
other CCD would pay more. Instead all 23 placements cost the same:
0.872–0.912 ns packed, SMT sibling included at 0.899, with the ratio
4.35–4.60x everywhere. No block of CPUs (for example 12–23) stood out as a
second chiplet. The guest's view of topology is flat by construction: sysfs
reports one L3 across all 24 CPUs. Pinning in the guest pins a thread to a
*virtual* CPU, and the host scheduler (Hyper-V) decides which physical core
that vCPU runs on. The guest cannot verify that its "sibling" pair `(2,3)`
runs on a host SMT pair, or that two vCPUs stay on one CCD. A flat sweep is
what you would expect if that mapping is not what the guest believes, or
not stable. A second possibility is that true SMT siblings would also keep
a penalty, because memory-ordering flushes happen inside one core too. The
two can't be separated from inside WSL2.

**The same cause is the most plausible explanation for the packed runs'
two levels.** Packed p50s cluster at **~0.85 ns** or **~1.3 ns** per
*process* (exp2: 0.87 / 1.30 / 0.85 / 1.09 / 0.84; exp4: 0.88 / 0.87 /
1.29 / 1.26 / 1.31), while padded never moves (0.196–0.200). Within a
process the level mostly holds; one exp2 run had a 0.774 min under a 1.299
p50, so a run can switch levels. The guest changed nothing between these
processes. The same CPUs, binary and code produced two distinct costs. A
host-side placement difference, such as the two vCPUs landing on the same
vs different CCDs, would produce exactly this pattern: the line-transfer
cost changes, but the guest-visible CPU numbers do not. This is a
hypothesis. It is the same unobservable variable that flattened Experiment
7, and it can only be tested on native Linux, where core IDs are physical.

## 9. What I learned

- False sharing needs no shared data. Two threads that are completely
  independent at the source level can still contend on the hardware,
  because the unit of coherence is the 64-byte line, not the variable. The
  bug is invisible in the code and only visible in the memory layout.
- The fix is a layout change, not a synchronization change:
  `alignas(64)` on the per-thread element type. On this CPU, 64 is exactly
  enough (the Experiment 6 cliff) and 128 buys nothing.
- With false sharing, adding threads can reduce total throughput. Here two
  writers did less combined work than one, and eight did less than two. A
  falsely-shared line is a serial resource disguised as parallel code, and
  "more cores → less total work" is its signature in a scaling test.
- The penalty's size depends on the kind of write. Plain stores can hide
  much of the line transfer behind the store buffer, which is why this
  benchmark shows ~5x rather than the ~50x a naive "one line transfer per
  increment" model predicts. That hiding is exactly what a locked atomic
  cannot do (Project 06).
- Validating the apparatus mattered again. The layout checks read *runtime
  addresses*, not the struct definitions, and the per-race count and pin
  checks ensure every timing came from the configuration it claims.
- Pinning inside a VM does not pin to hardware. Topology experiments
  (SMT, CCD) produce flat, meaningless results when the guest's CPU
  numbers are not physical cores. The run-to-run bimodality is a
  reminder that invisible host placement can move a result by ~50%.

## 10. Limitations

- **WSL2 / virtualized topology.** The biggest limitation. Experiment 7 and
  the packed-run bimodality both depend on physical placement that the
  guest cannot see or control. This project does not establish what
  false sharing costs across SMT siblings or across the 7900X's two CCDs
  on bare metal. It establishes only that it could not be measured here.
- **No hardware counters.** The store-buffer explanation for the ~5x
  magnitude is an inference from timing, not a measured count of
  coherence transfers or pipeline flushes (no `perf`/PMU access in WSL2).
- **One write pattern.** A tight loop of plain stores, one writer per
  counter, 100% writes. Real false sharing often involves a writer and a
  *reader* of neighboring fields, or writes interleaved with other work,
  and the cost per write would differ. This benchmark shows the effect's
  existence and shape, not its cost in a real hot path.
- **Throughput, not latency distribution.** The number is the mean cost per
  increment over a whole loop. It says nothing about the tail latency of
  a single write that happened to wait for the line, which is what an HFT
  hot path would care about.
- **Frequency not controlled.** Boost clocks are host-controlled, so
  absolute ns values (and the slight padded slowdown at 8 threads) include
  whatever frequency the host chose. Ratios within one run are more
  trustworthy than absolute values across runs.
- **Thread sweep capped at 8** (8 `uint64_t` fill one line) and uses one
  fixed CPU order (2, 4, …, 16).

## 11. Next experiment

Project 06 — Atomic Counter Benchmark replaces the plain `volatile` store
with `std::atomic::fetch_add` (and then a mutex). It directly tests this
project's store-buffer explanation. If a locked RMW must own the line at
every operation, the false-sharing penalty for atomics should be far larger
than the ~5x measured here, and a *truly* shared atomic counter should
behave like the packed case's worst end. Follow-ups specific to this
project, not required by the roadmap:
(1) rerun Experiment 7 on the planned `native` Linux environment, where
core IDs are physical, to get the real SMT-sibling and cross-CCD numbers
and to check whether the ~0.85 vs ~1.3 ns packed levels map to same-CCD vs
cross-CCD placement;
(2) a reader/writer variant, with one thread writing its counter and the
other only *reading* a neighbor in the same line, to measure false sharing
where only one side writes.

---

### Questions I can answer

- [x] Why can two threads that share no variables still slow each other
      down, and what exactly is being "shared"?
- [x] Why does `alignas(64)` on the element type fix it, and why is 64
      (not 128) sufficient on this CPU?
- [x] Why did one thread show no difference between packed and padded, and
      why was that check necessary before trusting the two-thread numbers?
- [x] Why did adding threads to a falsely-shared line *reduce* total
      throughput, and what does that say about the line as a resource?
- [x] Why was the penalty ~5x rather than the cost of one line transfer per
      increment, and what should change when the write is a locked atomic?
- [x] Why did SMT-sibling vs other-core placement make no difference here,
      and why is that a statement about WSL2 rather than about the CPU?
- [x] Why can the same binary on the same pinned CPUs produce two different
      packed costs in different processes?
