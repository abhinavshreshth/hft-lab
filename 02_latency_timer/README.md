# Project 02 — Latency Timer

Stage 1 · Concept: `steady_clock`, latency distributions, p50/p99

## 1. Objective

Project 01 answered *where* a thread runs. This project answers *how
consistently and how quickly code executes*, and establishes the standard
report used for the rest of the roadmap: `min, p50, p95, p99, p99.9, max`
instead of a single average. It also measures `std::chrono::steady_clock`'s
real tick granularity, filling in a placeholder left in
`docs/ENVIRONMENT.md`.

## 2. Environment

Base: `docs/ENVIRONMENT.md` (`wsl2`). No deltas — machine was otherwise idle
during all runs (AMD Ryzen 9 7900X, 24 logical CPUs, 1 NUMA node). This
project's Experiment 1 result (below) is what fills `docs/ENVIRONMENT.md`'s
previously-open `steady_clock resolution: (measure in Project 02)` line.

## 3. What I tested

Four executables, each a frozen lab-notebook entry like Project 01's, plus
one file that is deliberately **not** frozen or duplicated:

```
02_latency_timer/src/
├── latency_stats.hpp          shared percentile calculator (not an experiment)
├── exp1_clock_resolution.cpp  Experiment 1 — measure steady_clock's real tick size
├── exp2_naive_mean.cpp        Experiment 2 — baseline: one elapsed/N mean
├── exp3_percentile_capture.cpp Experiment 3 — + per-iteration timestamps, percentiles
└── exp4_compare.cpp           Experiment 4 — one run, reported both ways
```

Unlike Project 01, `latency_stats.hpp` (the percentile calculator) is
**shared** across every binary that needs it, via a plain `#include`. Project
01 duplicated its workload on purpose because the workload's evolution *was*
the point being taught; here the percentile calculator is the fixed
measurement apparatus being built, not part of an experimental progression —
sharing it is ordinary code organization, not the `common/` promotion
`docs/CONVENTIONS.md` §6 governs (that rule is about sharing code *between*
projects, not within one project's own `src/`).

```bash
cmake --build build
./build/02_latency_timer/02_exp1_clock_resolution
./build/02_latency_timer/02_exp2_naive_mean
./build/02_latency_timer/02_exp3_percentile_capture
./build/02_latency_timer/02_exp4_compare
```

`diff src/exp2_naive_mean.cpp src/exp3_percentile_capture.cpp` shows the one
change that matters: exp3 timestamps every iteration and reports a
distribution instead of a single average.

Reproducing the measurements (`benchmark/run.sh <binary> <reps> <label>`):

```bash
benchmark/run.sh 02_exp1_clock_resolution   5 clock_resolution
benchmark/run.sh 02_exp2_naive_mean         5 naive_mean
benchmark/run.sh 02_exp3_percentile_capture 5 percentile_capture
benchmark/run.sh 02_exp4_compare            5 compare
```

The workload for exp2/exp3/exp4 is identical: 20,000 `push_back`s onto a
`std::vector<uint64_t>` that is deliberately **not** pre-reserved, so
`std::vector`'s amortized growth actually happens — most pushes are O(1),
but roughly every `log2(N)`-th push triggers a real reallocation (malloc +
memcpy of the whole buffer + free of the old one). This is a genuine,
reproducible OS/allocator-level source of occasional slow iterations, not a
fabricated delay.

## 4. Baseline configuration

`02_exp2_naive_mean` — one `t0`/`t1` around the whole loop, reporting
`elapsed / N` as a single mean. This is exactly Project 01's measurement
method, applied here to a workload chosen to have a real tail. It captures
zero per-iteration data, so it is structurally incapable of seeing whether
that tail exists.

## 5. Modified configuration

`02_exp3_percentile_capture` — identical workload, with every `push_back`
individually timestamped into a pre-`reserve`d sample buffer, then reported
via `min/p50/p95/p99/p99.9/max`.

Hypothesis (written *before* measuring): the aggregate mean will look
unremarkable and similar in magnitude to exp2's, because it is dominated by
~20,000 fast pushes; the percentile report from the *same* workload will
reveal that a small number of iterations are dramatically slower than the
rest — visible at least in `max`, and plausibly in `p99.9` — even though the
mean cannot show it. Which exact percentile column catches it depends on how
many reallocations occur relative to sample count, so this is stated as a
prediction to confirm, not a guaranteed result.

## 6. Measurements

**Exp1 (clock resolution):** warmup = 100 discarded tick-changes (the first
`now()` calls pay a one-time vDSO/page-touch cost); 10,000 post-warmup
samples collected by busy-polling `now()` and recording the size of each
observed jump. Repetitions: 5.

**Exp2/3/4 (push_back workload):** warmup = **0** iterations — a deliberate
departure from exp1 and from the usual default. `std::vector` doubling
events are front-loaded (they occur at push counts 1, 2, 4, 8, 16, ...), so
discarding "the first N iterations" would delete most of the very events
under study. `docs/MEASUREMENT.md` explicitly carves out this exception:
warmup applies "unless per-iteration latency is itself the subject." 20,000
iterations per run, 5 repetitions per condition. The sample-collection buffer
(`samples_ns`) is pre-`reserve`d in exp3/exp4 so the measurement harness's
own vector growth cannot inject spikes into the data being measured. No
outliers are discarded anywhere — the outliers are the subject.

## 7. Results

### Clock resolution (Experiment 1, 5 runs)

| Metric | Value (ns) |
|---|---|
| min (practical resolution) | 10 — identical in all 5 runs |
| p50 | 20 |
| p95 | 20–30 |
| p99 | 20–40 |
| p99.9 | 1020–1370 |
| max | 1440–3440 |

`steady_clock`'s advertised representation is nanoseconds, but the real,
measured tick granularity on this machine is **10 ns**. This is the number
now recorded in `docs/ENVIRONMENT.md`.

### Baseline vs. modified (5 runs each, 20,000 push_backs/run)

| Metric | Baseline (exp2, naive mean) | Modified (exp3, percentiles) |
|---|---|---|
| Reported statistic | single mean only | full distribution |
| Mean (ns/push_back) | 7.19 – 10.02 (avg ≈ 8.5) | not reported (see exp4 for same-run comparison) |
| min (ns) | not captured | 10 |
| p50 (ns) | not captured | 20 |
| p95 (ns) | not captured | 20 |
| p99 (ns) | not captured | 20 |
| p99.9 (ns) | not captured | 1140 – 1650 |
| max (ns) | not captured | 46,911 – 87,832 |
| Reallocations observed | not captured | 16 (identical in all 5 runs) |

### Same-run comparison (Experiment 4, 5 runs)

| Metric | Framing A: naive mean | Framing B: percentiles (same data) |
|---|---|---|
| ns | 45.78 – 50.62 | p50 = 20, p99.9 = 1320–1570, max = 57,391–94,242 |

Raw output: `results/2026-09-09_clock_resolution.txt`,
`results/2026-09-09_naive_mean.txt`, `results/2026-09-09_percentile_capture.txt`,
`results/2026-09-09_compare.txt`

## 8. Why the results happened

**The aggregate mean hides the tail because it is a weighted average
dominated by the majority.** 16 reallocations out of 20,000 pushes is 0.08%
of iterations. Even though each reallocation costs tens of thousands of
nanoseconds, spread across 20,000 samples that only adds a few ns to the
mean — indistinguishable from ordinary run-to-run noise in exp2's baseline
(7.19–10.02 ns).

**`p99.9` is elevated but does not by itself capture the reallocation cost —
only `max` fully does, and the reason is arithmetic, not a flaw in the
method.** With n = 20,000, the nearest-rank `p99.9` index is
`ceil(0.999 × 20000) = 19980`, which is the **21st-largest** sample
(`n − rank + 1 = 21`). There are only 16 reallocation events, so they occupy
the top 16 positions — the p99.9 value (1140–1650 ns) is actually the 21st
biggest sample, one tier *below* the reallocation events themselves, and
reflects ordinary scheduling/cache jitter, not a reallocation. It is still
~60–80x the median (20 ns), which honestly signals "something is off in the
tail" — but the reallocations themselves are rare enough, relative to this
sample size, that only `max` (and the very top few ranks beyond p99.9) shows
their true cost (46,911–94,242 ns, roughly 3,000–5,000x the median). This is
an important, honest nuance: a single percentile is not automatically enough
to catch a rare event — the choice of percentile must be matched to how rare
the event is relative to the sample count.

**The reallocation count (16) matches the doubling-growth prediction.**
Growing a vector from empty to 20,000 elements by capacity doubling
(1, 2, 4, 8, ..., 16384) requires `ceil(log2(20000)) ≈ 15` growth steps from
the first push onward; 16 observed (including the very first allocation from
capacity 0) matches this exactly, and was identical across all 5 runs —
confirming the mechanism is deterministic given this allocator's growth
policy, not measurement noise.

**Experiment 4's "naive mean" (45.78–50.62 ns) is much higher than
Experiment 2's naive mean (7.19–10.02 ns) even though both average over the
same kind of workload.** This is the observer effect Project 01 already
found: exp4's loop calls `steady_clock::now()` twice per iteration (to
collect the per-iteration samples), while exp2's loop calls it zero times
per iteration (only once before and once after the whole loop). Each `now()`
call costs on the order of the clock resolution itself (~10-20 ns per
Experiment 1), so instrumenting every iteration measurably inflates the
total elapsed time being measured — exactly why exp2 and exp4 report
different "naive means" for nominally the same operation. This is also why
exp3's own median (20 ns) cannot be read as "a push_back costs 20 ns" — at a
10 ns clock resolution, a real push_back (well under a nanosecond, absent
reallocation) is unresolvable, so the reported 20 ns floor is dominated by
the cost of the two `now()` calls surrounding it, not the push_back itself.

## 9. What I learned

- A percentile answers "what value is X% of samples at or below," not "what
  is typical" — nearest-rank percentiles (used here) always name a real
  observed sample rather than interpolating a synthetic value between two.
- The mean is a poor summary for latency because it is linear in the data:
  a tiny fraction of huge outliers contributes only a tiny fraction to the
  sum, so they vanish into the average even though they may be the only
  numbers that matter operationally.
- Whether a given percentile (p99 vs p99.9 vs only max) actually reveals a
  rare event is arithmetic, not automatic: it depends on how the event's
  rate compares to `1 - p/100`. With rarer events than the chosen
  percentile's threshold, only a percentile closer to 100 (or max itself)
  will show it — this has to be checked, not assumed.
- `std::vector`'s amortized O(1) `push_back` is a *statement about the
  average*, not a promise about every call — the same distinction the mean
  vs. percentile argument is about, applied to a textbook complexity claim.
- Measuring per-iteration latency changes what you measure: adding two
  `now()` calls per iteration inflated the "naive mean" from ~8 ns to
  ~48 ns for nominally the same workload — an even sharper version of
  Project 01's instrumentation-cost finding.
- A clock's advertised type resolution (nanoseconds, for `steady_clock`) is
  not its real granularity; the only way to know the latter is to measure it
  directly, by polling until the value changes.
- A measurement's own buffer must be pre-sized before the timed region
  starts, or the harness's own allocation behavior contaminates the exact
  phenomenon (allocation latency) being studied.
- Warmup is not a universal default: when the rare events under study are
  concentrated at the start of a run (as doubling-driven reallocations are),
  warmup would delete the very thing being measured.

## 10. Limitations

- `std::vector`'s growth factor (here, libstdc++'s ~2x doubling) is
  implementation-defined; a different STL would produce a different
  reallocation count and different tail shape for the same `kIterations`.
- Single machine, single session, WSL2's virtualized kernel and clocksource
  may not match bare-metal timing or resolution — the measured 10 ns
  resolution is a `wsl2`-environment number, not a hardware constant.
- `kIterations = 20,000` was chosen so the ~15 predicted reallocations would
  land near the p99.9 boundary; as shown in section 8, they actually land
  just above it (in the top 16, not resolved by p99.9 itself). A different
  `kIterations` would shift this boundary and could make p99.9 (rather than
  only max) directly capture the reallocation cost.
- Only 5 repetitions per condition — enough to see the pattern is
  reproducible (the reallocation count was exactly 16 in all 5 runs), not
  enough to statistically bound the exact tail latency values, which varied
  run to run (max ranged from ~47 µs to ~94 µs across the two experiments).
- exp3's per-iteration timestamps cannot resolve costs below the ~10 ns
  clock resolution measured in Experiment 1; a real zero-reallocation
  `push_back` is almost certainly cheaper than what is reported for the
  "typical" (p50) case here.
- This does not test contended conditions (concurrent allocators, memory
  pressure) — only single-threaded, otherwise-idle-machine behavior.

## 11. Next experiment

Project 03 inherits this project's clock discipline (`steady_clock`,
distributions, not averages) as the standard measurement method for the rest
of the roadmap. A good follow-up specific to this project (not required by
the roadmap): rerun exp3 with the *workload* vector also pre-`reserve`d, to
show the tail disappearing entirely — proving the mechanism is reallocation,
not merely correlated with it.

---

### Questions I can answer
- [x] What is a percentile, and how does nearest-rank differ from
      interpolated percentiles?
- [x] Why does the mean hide tail latency?
- [x] How do you decide whether p99, p99.9, or only max will reveal a rare
      event?
- [x] What is a clock's real resolution, and how is it different from its
      advertised type precision?
- [x] Why must iteration count be much larger than clock resolution?
- [x] What causes a workload to be fast on average but occasionally slow
      (concretely: `std::vector` reallocation)?
- [x] Why can measuring per-iteration latency change the result you get
      (observer effect)?
- [x] Why did the warmup rule have to be applied differently here than in
      Project 01?
- [x] Why must a measurement's own sample buffer be pre-reserved before
      timing starts?
- [x] What would this analysis miss if only 100 iterations were run instead
      of 20,000?
