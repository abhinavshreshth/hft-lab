# Project 06 — Atomic Counter Benchmark

Stage 1 · Concept: atomics vs mutex

## 1. Objective

Project 05 gave every thread its *own* counter. This project makes them
share *one*. Once two threads write the same variable, something must make
each `++` indivisible, or increments are lost. The question is what that
costs. How much does a hardware atomic (`fetch_add`, a CAS loop) cost
compared with a lock (a spinlock, a `std::mutex`)? Measure it with no
contention, with two threads, and as threads are added up to 8. How does
each strategy fail as contention grows? And does Project 05's prediction
hold, that false sharing hurts locked atomics far more than plain stores?

## 2. Environment

Base: `docs/ENVIRONMENT.md` (`wsl2`). No deltas: AMD Ryzen 9 7900X, 12
cores / 24 logical CPUs, 64-byte cache lines, g++ 15.2 `-O2 -g`, machine
idle before the recorded runs (`top`: 99.6% idle).

The same WSL2 caveat as Project 05 applies. Guest CPU numbers are virtual
CPUs, and the guest cannot see which host core, or which of the 7900X's two
chiplets (CCDs), a vCPU is running on. `perf` and `strace` are not
installed, so there are no hardware counters and no syscall counts. Kernel
involvement is measured indirectly, through `getrusage` (§6).

## 3. What I tested

Six executables, all built on one shared apparatus:

```
06_atomic_counter/src/
├── atomic_counter.hpp        shared apparatus (not an experiment)
├── exp1_sanity.cpp           Experiment 1 — validate the apparatus, incl. a negative control
├── exp2_mutex.cpp            Experiment 2 — baseline: 2 threads, 1 counter, std::mutex
├── exp3_atomic.cpp           Experiment 3 — modified: 2 threads, 1 counter, fetch_add
├── exp4_compare.cpp          Experiment 4 — every strategy, 1 and 2 threads, ONE run
├── exp5_thread_scaling.cpp   Experiment 5 — 1..8 threads on one counter, + the sharded fix
└── exp6_false_sharing.cpp    Experiment 6 — Project 05's layouts with fetch_add instead of stores
```

`atomic_counter.hpp` follows `false_sharing.hpp`'s split:

- **`Counter`**: *how* one increment is made safe (the experimental
  variable). It is a C++20 concept rather than a virtual base class. In 05
  the variable was *where* the counter lived, so the hot loop ran on a raw
  pointer. Here the variable is the operation inside the hot loop, and a
  virtual call per increment would cost about as much as the thing being
  measured. The implementations, with what `-O2` actually emitted for each
  timed loop (checked with `objdump -d`):

  | Counter | Increment | Emitted in the timed loop |
  |---|---|---|
  | `PlainCounter` | `volatile` `v = v + 1`. Reference only, never shared | `mov (%rbx),%rax; add $1,%rax; mov %rax,(%rbx)` |
  | `LoadStoreCounter` | `atomic` relaxed `store(load() + 1)`. **Negative control** | the *same three instructions* as `plain` |
  | `FetchAddCounter` | `fetch_add(1, relaxed)` | `lock addq $0x1,(%rbx)` |
  | `CasCounter` | `compare_exchange_weak` loop, counting failures | `mov; lea; lock cmpxchg; jne retry` |
  | `SpinlockCounter` | test-and-test-and-set on an `atomic<bool>`, `_mm_pause` while spinning | `xchg %al,(%rbx)` … `pause` … `addq $1,8(%rbx); movb $0,(%rbx)` |
  | `MutexCounter` | `std::lock_guard<std::mutex>`, `++` | `call pthread_mutex_lock` … `addq` … `call pthread_mutex_unlock` |

- **`race<C>()`**: the fixed apparatus, the same as 05's `RaceRunner`. One
  pinned `std::thread` per requested CPU is released by a start barrier,
  and each thread times its own loop. Thread *i* increments `*counters[i]`.
  If all the pointers are the same, that is true sharing. If they are
  distinct, the counters are private (`PackedCounters` / `PaddedCounters`,
  05's layouts made generic over the counter type). Afterwards the distinct
  counters must sum to exactly `threads × iterations`.
- **`measure_interleaved()`**: races several configurations trial by
  trial, with the order rotating each round. It is used by Experiments 4–6
  so every comparison is made within one process.

`diff src/exp2_mutex.cpp src/exp3_atomic.cpp` shows the one code change
that matters: `MutexCounter counter;` → `FetchAddCounter counter;`.

```bash
cmake --build build
./build/06_atomic_counter/06_exp1_sanity
./build/06_atomic_counter/06_exp2_mutex          [cpuA cpuB]
./build/06_atomic_counter/06_exp3_atomic         [cpuA cpuB]
./build/06_atomic_counter/06_exp4_compare        [cpuA cpuB]
./build/06_atomic_counter/06_exp5_thread_scaling
./build/06_atomic_counter/06_exp6_false_sharing  [cpuA cpuB]
```

Reproducing the measurements (`benchmark/run.sh <binary> <reps> <label>`):

```bash
benchmark/run.sh 06_exp1_sanity          5 sanity
benchmark/run.sh 06_exp2_mutex           5 mutex
benchmark/run.sh 06_exp3_atomic          5 atomic
benchmark/run.sh 06_exp4_compare         5 compare
benchmark/run.sh 06_exp5_thread_scaling  5 thread_scaling
benchmark/run.sh 06_exp6_false_sharing   5 false_sharing
```

## 4. Baseline configuration

`06_exp2_mutex` pins two threads to CPU 2 (core 1) and CPU 4 (core 2), the
same placement as Project 05. Each thread does 5,000,000 increments of
**one shared counter** protected by a `std::mutex`. This is the right
baseline because it is what almost everyone writes first: correct, and
obvious to review.

## 5. Modified configuration

`06_exp3_atomic` uses the same harness, CPUs and loop, with
`std::atomic<uint64_t>::fetch_add` in place of lock / `++` / unlock.

Hypotheses, stated before the recorded runs. A short calibration run to
choose iteration counts came first. As in Project 05, magnitudes it
revealed are not claimed as predictions.

1. **Uncontended:** a `lock`-prefixed RMW costs several times a plain
   increment, because it cannot forward from the store buffer and must
   drain it. A mutex is lock + unlock, two atomic RMWs plus two calls, so
   it should cost ~2–3x a `fetch_add`. A spinlock is one RMW plus a plain
   store, so it should cost about the same as `fetch_add`.
2. **Two threads:** every strategy slows by roughly an order of magnitude,
   because the line now moves between cores on every few operations. CAS
   should be worse than `fetch_add`, since a failed attempt is a wasted
   round trip. The mutex should be clearly the worst: the losing thread
   sleeps in the kernel (`futex`), which shows up as voluntary context
   switches, and the spinlock should land between CAS and mutex.
3. **Scaling:** one shared line is a serial resource (05, Experiment 5), so
   *total* throughput should not rise with threads for any shared
   strategy. CAS failures per increment should grow with the number of
   competitors. A sharded counter (one padded slot per thread) should
   scale almost linearly.
4. **05's prediction:** with `fetch_add` the false-sharing ratio
   (packed/padded) should be far larger than plain stores' ~5x, because a
   locked RMW cannot hide the missing line behind the store buffer. A
   private-but-packed atomic counter should cost the same as a truly
   shared one.

## 6. Measurements

- **What is timed:** the same as Project 05. Each thread times its own
  loop with `steady_clock` from its release at the start barrier. The
  headline number is **slowest-thread ns/increment**, because the race ends
  when the last thread finishes. Thread creation, pinning and the barrier
  are outside the timed region. The loop is timed as a whole and divided:
  an uncontended increment (0.2–4 ns) is below `steady_clock`'s 10 ns
  resolution.
- **Aggregate throughput:** `threads ÷ slowest ns/increment`, the total
  increments completed per unit time across all threads, reported in
  M increments/s.
- **Per-thread `getrusage(RUSAGE_THREAD)`**, read just outside the timed
  loop and summed over threads:
  - voluntary context switches (the thread blocked, meaning it slept in
    the kernel),
  - involuntary context switches (the thread was preempted),
  - user vs kernel CPU time. *kernel share* = kernel ÷ (user + kernel).
    This is how syscalls are seen without `strace`.
- **CAS failures:** `CasCounter::increment()` returns its failed attempts
  and the runner sums them. For every other counter this is a compile-time
  0 and is optimized out, which the disassembly above confirms.
- **Iterations:** 20 M per thread uncontended (Experiment 4), 5 M for
  two-thread races (2–4), 10 M in Experiment 6, and 2 M per thread in the
  1–8 thread sweep. The shortest timed loop is ≥ 2.8 ms.
- **Warmup:** one discarded race per configuration (`kWarmupRaces = 1`).
- **Trials and repetitions:** 11 timed trials per configuration per
  process, reported as min / p50 / max. Each executable was run **5
  times**. The tables give the **mean of the per-process p50s**, with the
  range across processes where it matters.
- **Correctness checks on every race, warmup included:** the exact final
  count, and every thread on the CPU it asked for. All 25 `checks:` lines
  read `ok`, and all 5 sanity runs printed `ALL CHECKS PASSED`.
- **Outliers:** none discarded. The one visible disturbance is flagged
  under Experiment 5.

## 7. Results

### Experiment 1 — sanity (5/5 runs passed every check)

| Check | Result |
|---|---|
| sysfs line size · `atomic<uint64_t>` / `atomic<bool>` `is_always_lock_free` | 64 · true / true |
| `sizeof` plain / fetch_add / cas / spinlock / mutex counter (`std::mutex`) | 8 / 8 / 8 / 16 / 48 (40) |
| Packed = 8 counters in one line · Padded = one line each (plain and atomic) | PASS |
| **Negative control:** 2 threads × 2 M on `LoadStoreCounter`, 55 races | **every race lost updates: 14.9 % – 50.6 %** (per-run minimum 40.0–45.4 % in 4 of 5 runs) |
| fetch_add / cas / spinlock / mutex shared by 2 and by 8 threads | exact every time |

### Experiments 2–4 — two threads on one counter (CPUs 2 and 4)

| | Mutex (exp2) | fetch_add (exp3) |
|---|---|---|
| Slowest-thread p50, per process | 25.23, 24.34, 24.48, 24.07, 24.62 | 8.41, 9.19, 9.62, 8.45, 8.28 |
| **Slowest-thread p50, mean** | **24.55 ns** | **8.79 ns** |
| Aggregate throughput | 81.5 M/s | 228.3 M/s |
| Voluntary context switches per trial (10 M increments) | 60.4 | 0 |
| **Kernel share of thread CPU time** | **37.4 %** (32.9–40.2) | **0.0 %** |

Experiment 4, all strategies in one run (mean of 5 runs):

| Strategy | 1 thread ns/incr | vs plain | 2 threads, slowest p50 | 2 thr / 1 thr | Aggregate (M/s) | CAS failures / incr | Vol. ctx sw / trial | Kernel share |
|---|---|---|---|---|---|---|---|---|
| plain (reference) | 0.201 | 1.0x | — (would race) | — | — | — | — | — |
| **fetch_add** | **1.342** | 6.7x | **8.23** | 6.1x | **243.2** | 0 | 0 | 0.3 % |
| cas_loop | 1.719 | 8.6x | 21.69 | 12.6x | 92.4 | 0.603 | 0 | 0.1 % |
| spinlock | 1.526 | 7.6x | 23.28 (21.5–25.9) | 15.2x | 86.4 | 0 | 0 | 0.2 % |
| mutex | 4.145 | 20.6x | 25.76 (23.9–27.7) | 6.2x | 77.9 | 0 | 85.8 | **36.5 %** |

Fastest-thread p50 was within 1 % of slowest-thread p50 for every
two-thread strategy.

### Experiment 5 — 1 to 8 threads on one counter (CPUs 2, 4, …, 16, one per physical core)

Aggregate throughput, M increments/s (mean of 5 runs):

| Threads | fetch_add | cas_loop | spinlock | mutex | **sharded** (padded per-thread fetch_add) |
|---|---|---|---|---|---|
| 1 | 706.7 | 569.5 | 640.3 | 234.4 | 689.7 |
| 2 | 237.2 | 93.7 | 89.0 | 83.5 | 1379.5 |
| 3 | 222.7 | 97.0 | 68.3 | 106.1 | 1977.8 |
| 4 | 221.7 | 96.6 | 45.5 | 79.5 | 2690.2 |
| 5 | 198.3 | 96.9 | 35.3 | 69.0 | 2905.4 † |
| 6 | 189.3 | 96.7 | 27.2 | 61.1 | 3379.9 † |
| 7 | 180.6 | 89.1 | 17.7 | 55.1 | 4412.2 |
| 8 | **155.4** | **66.7** | **12.0** | **51.5** | **5210.7** |

† Run 5 of 5 had sharded at 4.2 ns/increment at 5 and 6 threads, against
1.47–1.59 in every other run and at every other thread count in that same
run. It was a transient disturbance on a contender that shares nothing.
Without run 5 the means are 3334 and 3865.

What goes with those throughputs:

| Threads | CAS failures / incr | Mutex voluntary ctx sw / trial | Mutex kernel share | Slowest / fastest thread, fetch_add (ns) |
|---|---|---|---|---|
| 2 | 0.587 | 23.5 | 37.7 % | 8.44 / 8.40 |
| 3 | 0.572 | 409 | 40.2 % | 13.58 / 13.00 |
| 4 | 0.578 | 8,540 | 47.6 % | 18.19 / 16.33 |
| 5 | 0.493 | 18,800 | 47.6 % | 26.77 / 19.25 |
| 6 | 0.498 | 32,165 | 55.7 % | 33.12 / 25.01 |
| 7 | 0.504 | 42,780 | 67.0 % | 39.54 / **14.49** |
| 8 | 0.651 | 49,086 | **74.9 %** | 51.55 / 32.10 |

Per-thread p50 at 8 threads, ns/increment (mean of 5 runs):

| CPU (guest core) | 2 (1) | 4 (2) | 6 (3) | 8 (4) | 10 (5) | 12 (6) | 14 (7) | 16 (8) |
|---|---|---|---|---|---|---|---|---|
| fetch_add | 51.1 | 50.5 | 50.2 | 49.8 | 48.7 | **43.2** | **35.6** | **43.1** |
| cas_loop | 115.2 | 108.4 | 105.5 | 106.7 | 108.9 | 104.4 | 99.2 | 99.0 |
| spinlock | 657.3 | 644.4 | 649.5 | 660.1 | 644.0 | 658.1 | 640.9 | 644.1 |
| mutex | 154.4 | 154.0 | 154.6 | 154.6 | 154.1 | 155.1 | 154.1 | 154.1 |

For fetch_add, CPU 14 was the fastest thread in all 5 runs.

### Experiment 6 — false sharing: plain vs atomic (CPUs 2 and 4, mean of 5 runs)

| Config | Slowest-thread p50 (ns/incr) | Range across runs |
|---|---|---|
| plain packed | 1.023 | 0.850 – 1.126 |
| plain padded | 0.199 | 0.192 – 0.206 |
| fetch_add packed | 8.746 | 7.727 – 9.287 |
| fetch_add padded | 1.368 | 1.351 – 1.385 |
| fetch_add **shared** (one counter) | 8.259 | 7.864 – 8.495 |

| | Ratio packed / padded | Penalty in ns (packed − padded) |
|---|---|---|
| plain | **5.1x** (4.1–5.7) | +0.82 |
| fetch_add | **6.4x** (5.7–6.7) | **+7.38** |
| fetch_add packed / shared | 1.06x (0.98–1.11) | — |

Raw output: `results/2026-09-29_sanity.txt`, `results/2026-09-29_mutex.txt`,
`results/2026-09-29_atomic.txt`, `results/2026-09-29_compare.txt`,
`results/2026-09-29_thread_scaling.txt`, `results/2026-09-29_false_sharing.txt`

## 8. Why the results happened

**H1 was confirmed. H2 was half right: the mutex was not "the worst
because it sleeps". H3 was confirmed except for the CAS prediction, which
was refuted. H4 was refuted as a ratio but confirmed in nanoseconds.**

**Atomicity of each access is not atomicity of the increment (Experiment
1).** `LoadStoreCounter` uses only atomic operations and has no data race.
It compiled to *exactly* the same three instructions as the plain volatile
increment, and it lost up to half of all increments. Every step is
indivisible; the read-modify-write as a whole is not. Why the loss clusters
at 40–50 %, and why the worst race in two of the five runs lost exactly
2,000,000 (one whole thread's worth): this is the store-to-load forwarding from Project 05 again. While
the threads overlap, each core's load is mostly served from its *own* most
recent store, so each thread effectively counts its own increments. The
final value is roughly whichever thread's count was written last. The
negative control is what gives "exact every time" for the other four
strategies any meaning: the check demonstrably can fail.

**Uncontended, a lock prefix costs ~7x a plain increment (H1).** Plain:
0.20 ns, about one cycle, because the dependent load comes out of the
store buffer. `lock add`: 1.34 ns. A locked RMW must drain the store buffer
and own the line for the whole load-add-store, so consecutive increments
cannot overlap. That is about 7 cycles, *if* the core is at ~5.3 GHz boost,
which WSL2 doesn't let the guest observe. The spinlock (1.53 ns) is one
`xchg` plus a plain release store, so it lands next to `fetch_add` as
predicted. CAS (1.72 ns) adds a load in front of its `lock cmpxchg`. The
uncontended mutex is 4.15 ns, 3.1x `fetch_add`. That matches "two atomic
RMWs plus two calls into glibc", and it involves **no syscall at all**
(kernel share 0.0 % at 1 thread).

**Contended, `fetch_add` wins by ~3x, and the mutex's problem is the
kernel, not sleeping (H2).** With two threads, `fetch_add` slowed 6.1x to
8.2 ns (243 M/s total). The line moves between the cores, but each `lock
add` is one short, self-contained ownership, so it degrades gracefully. CAS
is 2.6x worse than `fetch_add`: its plain load first pulls the line in
Shared state, and the `cmpxchg` must then upgrade it to Exclusive. That is
two coherence transactions per attempt instead of one, plus 0.6 failed
attempts per increment. The spinlock and the mutex landed together at
~23–26 ns, with overlapping ranges across runs. (The calibration run had
them in the opposite order, which says they are not separable here.) The
hypothesis that the mutex would be worst *because its losers sleep* was
wrong: across 10 M contended increments per trial, its threads slept only
~60–86 times. Yet **37 % of their CPU time was in the kernel**. The likely
mechanism is glibc's futex protocol. A thread that finds the lock taken
marks it contended and calls `futex_wait`, which usually returns at once
because the lock was already released before the kernel checked. An
unlocker that sees the contended mark calls `futex_wake` whether or not
anyone is asleep. So the mutex pays syscalls without sleeping. That is an
inference from the kernel time share: the syscalls themselves couldn't be
counted here (no `strace`/`perf`).

**One shared line caps total throughput, whatever protects it (H3).** No
shared strategy got faster with more threads. `fetch_add` declined
steadily from 237 to 155 M/s between 2 and 8 threads. The line can only be
owned by one core at a time, and each handoff gets slightly more expensive
as more cores queue for it. The sharded counter is still a locked atomic
per increment, but it shares nothing and scaled from 690 to 5,211 M/s
(7.6x on 8 cores). Comparing it with shared `fetch_add` separates the two
costs: "atomic" costs ~1.4 ns, and "shared" is what costs the other ~50.

**The strategies fail in different ways.**
- *Spinlock: collapse.* 640 → 12 M/s, 53x worse at 8 threads than at 1
  and 13x worse than `fetch_add`. Every release store invalidates every
  spinner's Shared copy. Every spinner then re-reads, sees "free", and
  fires an `xchg`. N−1 of those `xchg`s fail, but each failure still takes
  the line exclusively, including away from the new holder, who needs it
  again for `++v` (`v` sits in the *same* line as the flag) and for the
  release. The line transfers per increment grow with the number of
  spinners. Fairness was fine (per-thread times within ~3 %); raw
  throughput was the problem.
- *Mutex: it degrades into the kernel.* 84 → 52 M/s. Kernel share grows
  from 38 % to 75 %, and real sleeps from 24 to ~49,000 per trial. Past 3
  threads the losers genuinely sleep. Sleeping acts as backoff: it takes
  them out of the fight for the line, which is why the mutex degrades
  *gracefully* and beats the spinlock 4.3x at 8 threads. It even improved
  from 2 to 3 threads (84 → 106 M/s): once one thread sleeps, the other two
  get longer uncontended bursts. Every thread ended within ~1 % of the
  others, so the futex queue is fair at race granularity.
- *CAS: flat, not collapsing, and the refuted sub-prediction explains
  why.* Failures per increment stayed at **0.49–0.65 from 2 to 8
  threads**. The hypothesis expected them to multiply. On x86 a `lock
  cmpxchg` takes the line exclusively and writes it even when the
  comparison *fails*, and it hands back the current value. So the retry
  immediately after a failure runs on a line this core already owns, and
  almost always succeeds. Failures therefore cannot cascade, and the cost
  stays at roughly "one extra transaction per contended increment".

**H4: atomics suffer more false sharing in nanoseconds, not as a
multiple.** Project 05 predicted the packed/padded ratio for `fetch_add`
would be "far larger" than plain stores' ~5x. It was 6.4x vs 5.1x, the
same order. The reasoning behind the prediction was right, but the metric
was wrong. The numerator did grow enormously: the false-sharing *penalty*
is +7.4 ns per locked increment vs +0.8 ns per plain store, **9x larger**,
because a locked RMW cannot keep running while the line is away. The
denominator grew by almost the same factor, because an uncontended locked
RMW is itself ~7x a plain store. The second half of the prediction held:
private-but-packed atomic counters cost the same as one truly shared
counter (1.06x). The hardware cannot tell "two variables in one line" from
"one variable"; the unit it arbitrates is the line.

**An unexplained asymmetry that may be the CCD boundary.** At 8 threads on
shared `fetch_add`, the three threads on guest cores 6–8 averaged 40.6 ns
per increment vs 50.1 ns on cores 1–5. CPU 14 was the fastest thread in all
5 runs. At 7 threads the fastest thread (14.5 ns) was 2.7x faster than the
slowest (39.5). The boundary falls exactly where a 6 + 6 split of the
7900X's cores would put it, if guest cores 0–5 and 6–11 map to the two
CCDs. One mechanism consistent with it: while the line is in the other
chiplet it bounces cheaply among the minority's few cores before crossing
back, so those cores get more turns. This is the first placement-dependent
effect this lab has measured under WSL2; Project 05's placement sweep was
flat. It is a hypothesis: the guest cannot see the vCPU → CCD mapping, and
CAS/spinlock/mutex showed no such split.

## 9. What I learned

- `std::atomic` makes each *access* indivisible, not a *sequence* of
  accesses. `x.store(x.load() + 1)` is race-free, compiles to the same
  code as a plain increment, and loses half the updates. Only a
  read-modify-write instruction (`fetch_add`, `cmpxchg`) makes `++` atomic.
- For a counter, `fetch_add` is the right tool at every contention level
  measured: ~7x a plain increment when uncontended, ~3x faster than any
  alternative when shared, and graceful as threads are added. CAS is for
  updates that can't be expressed as one RMW, and it costs ~2.6x
  `fetch_add` under contention.
- A mutex is cheap when uncontended (4 ns, no syscall), not a disaster at
  two threads, and degrades into the kernel as contention grows. The
  expensive part is the futex syscalls, not only the sleeps. An HFT hot
  path cares about that: a syscall is exactly the kind of unbounded,
  unpredictable latency a pinned hot thread must avoid.
- A naive spinlock is the worst choice under real contention. "Never
  sleeps" is not the same as "fast": without backoff, the waiters' own
  traffic starves the holder. The mutex's sleeping is, in effect, backoff.
- Sharing, not atomicity, is the cost. The padded per-thread atomic
  scaled 7.6x on 8 cores while every shared variant went *down*. The fix
  for a contended counter is to stop sharing it: shard it, and sum when
  reading.
- Ratios can mislead. The atomic false-sharing ratio looked the same as
  05's; the absolute penalty was 9x larger. Report both.
- Validate the check itself. The negative control is what makes
  "exact every time" a result instead of an assumption.

## 10. Limitations

- **WSL2 / virtualized topology.** CPU numbers are vCPUs, so the CCD
  asymmetry in Experiment 5 cannot be confirmed here, and absolute
  handoff costs include whatever placement the host chose.
- **No syscall or PMU counts.** The futex explanation for the mutex's
  kernel time is an inference from `getrusage` kernel-time share. No
  `futex` calls were counted, and no coherence traffic was measured.
- **One glibc mutex type.** The default `pthread_mutex` (no adaptive
  spinning). An adaptive mutex (`PTHREAD_MUTEX_ADAPTIVE_NP`) or a
  different libc would behave differently under contention.
- **One naive spinlock.** TTAS with `_mm_pause` and no backoff, with the
  protected value in the lock's own cache line. Exponential backoff, a
  ticket lock or a queue lock (MCS) would change Experiment 5
  substantially. The spinlock result condemns *this* spinlock, not
  spinning in general.
- **A worst-case workload.** Threads do nothing but increment. Real code
  does work between increments, which lowers contention and would narrow
  every gap measured here.
- **Throughput, not per-operation latency.** The numbers are mean cost per
  increment over a whole loop. They say nothing about the tail: for
  example, how long the single worst `lock()` took. For a mutex that tail
  includes a sleep and a wakeup, which is exactly what an HFT hot path
  would care about.
- **Frequency not controlled.** Absolute ns values include host-chosen
  boost clocks. Ratios within one run are more trustworthy than absolute
  values across runs.
- **One disturbance** (run 5, sharded, 5–6 threads) is included in the
  means and flagged in §7.

## 11. Next experiment

Project 07 — Memory Ordering Lab. Every atomic here used `relaxed` (the
counters) or `acquire`/`release` (the locks), and on x86 a locked RMW is a
full barrier whatever order is requested. So this project could not see
what the orderings cost or what they guarantee. 07 asks what
`acquire`/`release`/`seq_cst` actually promise, and where the difference
becomes measurable: a `seq_cst` *store* compiles to `xchg` / `mfence` on
x86, a `release` store to a plain `mov`. It also asks what goes wrong when
ordering is too weak. Follow-ups specific to this project, not required by
the roadmap:
(1) rerun Experiment 5 on `native` Linux with physical core IDs, to test
whether the cores 6–8 asymmetry is the CCD boundary;
(2) per-operation latency percentiles for `lock()` vs `fetch_add` under
contention, to measure the mutex's tail instead of its mean;
(3) a spinlock with exponential backoff and a separate line for the
protected value, to see how much of the spinlock collapse was naivety.

---

### Questions I can answer

- [x] Why does `x.store(x.load() + 1)` lose updates even though every
      operation in it is atomic, and what does `fetch_add` do differently
      at the instruction level?
- [x] Why is an uncontended `lock add` ~7x a plain increment, and why is an
      uncontended mutex only ~3x `fetch_add` and syscall-free?
- [x] Why is a CAS loop ~2.6x slower than `fetch_add` under contention, and
      why did its failure rate *not* grow with thread count?
- [x] Why did the two-thread mutex spend over a third of its CPU time in
      the kernel while almost never sleeping?
- [x] Why does total throughput on one shared counter fall as threads are
      added, whatever protects it, and why does sharding fix it?
- [x] Why did the spinlock collapse at 8 threads while the mutex degraded
      gracefully?
- [x] Was Project 05's prediction about atomic false sharing right, and why
      does the answer depend on whether you look at the ratio or the ns?
