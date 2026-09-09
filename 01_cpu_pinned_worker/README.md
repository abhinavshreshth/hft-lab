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

The spec's four experiments map onto **two runtime configurations** of one
worker function, because Experiment 2 is instrumentation shared by both and
Experiment 4 is an analysis, not a program:

```
Project 01
├── Experiment 1: Unpinned Worker      -> ./01_cpu_pinned_worker unpinned
│   └── no affinity; establish baseline
├── Experiment 2: Detect Current CPU   -> instrumentation inside worker()
│   └── initial CPU, CPUs observed, migration count (used by both modes)
├── Experiment 3: Pin Worker           -> ./01_cpu_pinned_worker pinned [cpu]
│   └── select CPU, apply affinity, verify the worker stays there
└── Experiment 4: Compare              -> ./01_cpu_pinned_worker compare
    └── both back to back; analysis in sections 7-8 below
```

The workload is identical in every mode: 2,000,000,000 iterations of a
`volatile` accumulator, so the compiler cannot optimize the loop away (see
`docs/MEASUREMENT.md`). Each condition was run 5 times. Every run records
starting CPU, final CPU, migrations detected, elapsed wall-clock time, and the
accumulator value (proving the loop actually executed).

## 4. Baseline configuration

`./01_cpu_pinned_worker unpinned` — `worker(std::nullopt)`, no call to
`pthread_setaffinity_np`. The thread starts wherever the scheduler places it
and may migrate freely among all 24 CPUs.

## 5. Modified configuration

`./01_cpu_pinned_worker pinned 4` — `worker(4)`: before the loop, the thread
calls `pthread_setaffinity_np` with a `cpu_set_t` containing only CPU 4,
restricting the scheduler to that one CPU.

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
| CPUs observed (5 runs) | 4, 8, 15, 16, 17 (5 distinct placements) | 4 (always) |
| Total migrations (5 runs) | 0 | 0 |
| Elapsed time — min / mean / max (ms) | 757.57 / 763.88 / 766.89 | 764.10 / 765.26 / 767.27 |

Raw output: `results/2026-09-09_unpinned.txt`, `results/2026-09-09_pinned.txt`

5 reps proved too few to observe any migration, so a longer unpinned capture
was run specifically to test whether the counter fires at all:

| Migration hunt (20 unpinned runs) | Result |
|---|---|
| Runs with >=1 migration | 1 of 20 |
| The migration observed | CPU 22 -> 23 |
| Distinct starting CPUs | 4, 7, 9, 10, 11, 14, 15, 16, 19, 22 |

Raw output: `results/2026-09-09_unpinned_migration_hunt.txt`

CPU 22 and CPU 23 are SMT siblings of the same physical core (core 11,
`thread_siblings_list = 22-23`), so even that one migration was a move between
hyperthreads of the *same* core — sharing L1 and L2 — not a move to a
different physical core.

## 8. Why the results happened

**Placement became constant under pinning** because `pthread_setaffinity_np`
restricts the scheduler's choice set to a single CPU — there is nowhere else
for the thread to run, so every run starts and stays on CPU 4.

**Migration was rare — 0 of 5 in the headline runs, 1 of 20 in the dedicated
hunt — even unpinned**, because the machine has 24
logical CPUs and was otherwise idle. The Linux scheduler avoids migrating a
running thread unless there's a load-balancing reason to — moving it costs
cache locality (the thread's working set is warm in the old CPU's L1/L2) for
no benefit when free CPUs are already plentiful. Migration would be expected
to increase sharply under contention (many runnable threads competing for
few CPUs) — not tested here. It is worth noting the single migration observed
stayed within one physical core (SMT sibling 22 -> 23), which is the cheapest
kind of move available: L1 and L2 are shared between siblings, so the thread's
warm working set followed it. A migration to a different physical core would
be the expensive case, and we did not observe one at all.

**Elapsed time showed no real difference.** The two ranges (757.57–766.89 vs
764.10–767.27) overlap, the means differ by ~1.4 ms out of ~765 ms (0.2%), and
across sessions the sign of that difference keeps flipping — an earlier 3-run
session had pinned ~1% *slower*, an intermediate capture had it marginally
*faster*, this one has it ~0.2% slower. That reversal is itself the evidence:
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
