# Project 01 — CPU-Pinned Worker

Stage 1 · Concept: `std::thread`, CPU affinity

## 1. Objective

Understand where a worker thread actually executes: whether Linux moves it
between CPUs on its own (migration), and what changes when it is explicitly
restricted to one CPU (affinity/pinning).

## 2. Environment

Base: `docs/ENVIRONMENT.md` (`wsl2`). No deltas — machine was otherwise idle
during all runs (AMD Ryzen 9 7900X, 24 logical CPUs, 1 NUMA node).

## 3. What I tested

One executable per experiment, mirroring the four experiments in the spec.
Each file is a self-contained lab-notebook entry, meant to be read on its own
and **diffed against the previous one** to see exactly what that step added:

```
01_cpu_pinned_worker/src/
├── exp1_unpinned.cpp     Experiment 1 (docx §6)  — run worker, no affinity, baseline timing
├── exp2_detect_cpu.cpp   Experiment 2 (docx §9)  — + sched_getcpu, CPUs observed, migrations
├── exp3_pinned.cpp       Experiment 3 (docx §11) — + cpu_set_t affinity, verify it stays put
└── exp4_compare.cpp      Experiment 4 (docx §13) — both conditions back to back
```

```bash
cmake --build build
./build/01_cpu_pinned_worker/01_exp1_unpinned
./build/01_cpu_pinned_worker/01_exp2_detect_cpu
./build/01_cpu_pinned_worker/01_exp3_pinned 4
./build/01_cpu_pinned_worker/01_exp4_compare 4
```

`diff src/exp2_detect_cpu.cpp src/exp3_pinned.cpp` is the clearest statement of
what pinning actually is: a `cpu_set_t`, two macros, and one call.

The workload loop is duplicated across the four files on purpose. These are
frozen artifacts that will never need to change together, and factoring them
into a shared header would hide the very progression the files exist to show.

Reproducing the measurements (`benchmark/run.sh <binary> <reps> <label> [args]`):

```bash
benchmark/run.sh 01_exp2_detect_cpu 5  unpinned
benchmark/run.sh 01_exp3_pinned     5  pinned 4
benchmark/run.sh 01_exp2_detect_cpu 20 unpinned_migration_hunt
```

The workload is identical in every mode: 2,000,000,000 iterations of a
`volatile` accumulator, so the compiler cannot optimize the loop away (see
`docs/MEASUREMENT.md`). Each condition was run 5 times. Every run records
starting CPU, final CPU, migrations detected, elapsed wall-clock time, and the
accumulator value (proving the loop actually executed).

## 4. Baseline configuration

`01_exp2_detect_cpu` — no call to `pthread_setaffinity_np`. The thread starts
wherever the scheduler places it and may migrate freely among all 24 CPUs.

## 5. Modified configuration

`01_exp3_pinned 4` — before the loop, the thread calls
`pthread_setaffinity_np` with a `cpu_set_t` containing only CPU 4, restricting
the scheduler to that one CPU.

Hypothesis (written *before* measuring): pinning will make CPU placement
constant (always CPU 4, 0 migrations) but will **not** measurably change
execution time, because the machine is idle — there is no contention for
pinning to relieve. This matches the spec's own point that "CPU affinity is a
control mechanism that must be measured, not blindly applied."

## 6. Measurements

Warmup: none — this project measures one long (~0.77s) fixed-work run, not
many short iterations, so per-call warmup effects are negligible relative to
the run length. Iterations: 2,000,000,000 (fixed, per run). Repetitions: 5 per
condition. CPU checked every 1,000,000 iterations via `sched_getcpu()`.

## 7. Results

| Metric | Unpinned | Pinned (CPU 4) |
|---|---|---|
| CPUs observed (5 runs) | 2, 4, 10, 11 (4 distinct placements) | 4 (always) |
| Total migrations (5 runs) | 0 | 0 |
| Elapsed time — min / mean / max (ms) | 762.18 / 764.38 / 768.33 | 763.54 / 764.99 / 766.85 |

Raw output: `results/2026-09-09_unpinned.txt`, `results/2026-09-09_pinned.txt`

5 reps proved too few to observe any migration, so a longer unpinned capture
was run specifically to test whether the counter fires at all:

| Migration hunt (20 unpinned runs) | Result |
|---|---|
| Runs with >=1 migration | 4 of 20 |
| Migrations observed | 6→7, 6→7, 23→22, 1→0 |
| **Of those, within one physical core** | **4 of 4** |
| Distinct starting CPUs | 0, 1, 2, 4, 6, 17, 18, 19, 20, 21, 23 |

Raw output: `results/2026-09-09_unpinned_migration_hunt.txt`

### Instrumentation cost (observer effect)

Experiment 1 has no CPU check in its loop; Experiments 2-4 do. Same workload,
very different runtime:

| Loop body | Elapsed | Cost |
|---|---|---|
| `sink += i` only (exp1) | 381 ms | baseline |
| `+ if (i % 1048576 == 0)` — 2^20, compiles to a single `AND` | 569 ms | +188 ms (branch + check) |
| `+ if (i % 1000000 == 0)` — what exp2-4 actually run | 760 ms | +191 ms more (costly modulo) |

Measuring *where* the thread runs **doubled how long it takes**.

Every migration observed was between **SMT siblings of the same physical
core**, checked against `/sys/devices/system/cpu/cpuN/topology/thread_siblings_list`
(pairs 6-7, 22-23, 0-1). Not one migration crossed to a different physical core.

## 8. Why the results happened

**Placement became constant under pinning** because `pthread_setaffinity_np`
restricts the scheduler's choice set to a single CPU — there is nowhere else
for the thread to run, so every run starts and stays on CPU 4.

**Migration was rare — 0 of 5 in the headline runs, 4 of 20 in the dedicated
hunt — even unpinned**, because the machine has 24
logical CPUs and was otherwise idle. The Linux scheduler avoids migrating a
running thread unless there's a load-balancing reason to — moving it costs
cache locality (the thread's working set is warm in the old CPU's L1/L2) for
no benefit when free CPUs are already plentiful. Migration would be expected
to increase sharply under contention (many runnable threads competing for
few CPUs) — not tested here.

**Every migration stayed inside one physical core** (4 of 4). That is the
cheapest move available: SMT siblings share L1 and L2, so the thread's warm
working set followed it across. The costly case — migration to a different
physical core, with cold L1/L2 — was never observed at all. So the migration
penalty the spec warns about was never actually paid here, which is a further
reason pinning showed no time benefit: there was no damage for it to prevent.

**Instrumentation doubled the workload, and half of that was self-inflicted.**
`sched_getcpu()` is not the cause — it runs 2,000 times out of 2,000,000,000.
The cost is the `i % kCheckInterval` test itself, executed every single
iteration. Worse, `1,000,000` is not a power of two, so `%` cannot become a
bitmask: the compiler emits a multiply-high/shift/multiply/subtract sequence,
which roughly doubles the added cost versus a power-of-two interval (569 ms vs
760 ms measured). In a loop whose body was otherwise one add and one store,
that dominates.

This is the observer effect in miniature, and it is the reason Project 01's
`exp1` timing is **not** comparable to `exp2`/`exp3`/`exp4` timings. It does
**not** invalidate the pinned-vs-unpinned comparison, because exp2 (unpinned)
and exp3 (pinned) carry byte-for-byte identical instrumentation — the overhead
is present in both arms and cancels.

**Elapsed time showed no real difference.** The two ranges (762.18–768.33 vs
763.54–766.85) overlap, the means differ by ~0.6 ms out of ~765 ms (0.08%), and
across sessions the sign of that difference keeps flipping — an earlier 3-run
session had pinned ~1% *slower*, an intermediate capture had it marginally
*faster*, this one has it 0.08% slower. That reversal is itself the evidence:
the effect size is smaller than the run-to-run noise, so per
`docs/MEASUREMENT.md`'s honesty rule, the correct conclusion is **no
measurable latency difference**, not a direction. This is consistent with the
hypothesis: pinning controls *where* a thread can run, it does not by itself
make single-threaded arithmetic faster on an uncontended machine.

## 9. What I learned

- A thread and a process are different scheduling units — the OS moves
  threads, and a multi-threaded process can have threads spread across many
  CPUs simultaneously.
- `sched_getcpu()` is a snapshot, not a promise; you must sample it
  repeatedly over a sustained workload to observe migration at all.
- `std::thread` has no portable affinity API — real affinity control goes
  through the pthread API (`pthread_setaffinity_np` on `t.native_handle()`,
  or `pthread_self()` from inside the thread itself).
- Pinning is a *constraint* (restrict allowed CPUs), not an *optimization* —
  it produces consistency (deterministic placement), which is a different
  property from throughput or latency, and the two must be measured
  separately.
- Naive fixed-iteration-count loops can be silently gutted by the optimizer
  if the loop body has no real, unpredictable-to-the-compiler work; a
  `volatile` accumulator (or an explicit "don't optimize this" escape) is
  needed to guarantee the measured time reflects real work.
- Measuring something can change it. Adding a CPU check every 1,000,000
  iterations doubled the runtime of the loop being measured. A comparison
  survives this only if both arms carry identical instrumentation — which is
  why exp2 vs exp3 is still valid while exp1 vs exp2 is not.
- Integer `%` by a non-power-of-two constant is not cheap. Choosing 1,048,576
  (2^20) instead of 1,000,000 would recover ~25% of this loop's runtime for
  free, because the compiler can then use a single bitwise `AND`. Round
  numbers in decimal are not round numbers to a CPU.
- Pinning *can* actively hurt performance, even though it didn't here: if the
  chosen CPU is a bad pick (e.g. its SMT sibling is running something else
  busy — CPU 4's sibling is CPU 5 on this machine, both on physical core 2 —
  or it's a CPU with unrelated load already on it), pinning forces the
  thread to stay there instead of letting the scheduler move it to a free
  core. Unpinned, a busy thread can be load-balanced away from contention;
  pinned, it cannot. This project's idle machine never exercised that path —
  it's a case where pinning helps only once you've verified the target CPU is
  actually a good one.

## 10. Limitations

- The check interval (1,000,000) was chosen for readability, not speed, and
  costs ~25% of the loop's runtime versus a power-of-two interval. The code
  still ships that way so the captured results stay reproducible; switching to
  a countdown counter (`if (--next_check == 0)`) would remove the modulo
  entirely and is the obvious fix if these numbers are ever re-based.
- Single machine, single session, only 5 repetitions per condition — not
  enough to statistically bound a small effect, only enough to see that any
  effect is smaller than run-to-run noise.
- WSL2's scheduler runs inside a virtualized kernel; behavior (migration
  frequency in particular) may not match bare-metal Linux.
- The machine was idle by intent, but "idle" wasn't independently verified
  (no `top`/`htop` snapshot taken alongside each run) — some noise could come
  from background OS/WSL activity, not just the scheduler's own choices.
- Only one pinned target (CPU 4) was tested; different CPUs (e.g. a CPU
  sharing an L3 slice vs. not, given this is a single-L3-domain part here)
  were not compared.
- This says nothing about *contended* behavior — the interesting case for
  migration and for pinning's benefit — since the machine was deliberately
  idle throughout.

## 11. Next experiment

Project 02 — Latency Timer: replace this project's single wall-clock
measurement with a proper per-iteration latency distribution (min/p50/p95/
p99/p99.9/max), which is the measurement foundation the rest of the roadmap
depends on. A good follow-up specific to this project (not required by the
roadmap) would be re-running this comparison under artificial contention
(e.g. saturating the other 23 CPUs) to see whether pinning's benefit becomes
measurable once there is something to protect against.

---

### Questions I can answer
- [x] What is a thread?
- [x] What is the difference between a process and a thread?
- [x] What is a CPU core?
- [x] What is a logical CPU?
- [x] What does the Linux scheduler do?
- [x] What is CPU migration?
- [x] Why can CPU migration affect latency?
- [x] What is CPU affinity?
- [x] What does thread pinning accomplish?
- [x] Does CPU pinning guarantee better performance?
- [x] Why might pinning actually hurt performance?
- [x] Why is CPU placement important in low-latency systems?
