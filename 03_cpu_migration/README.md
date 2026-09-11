# Project 03 — CPU Migration Detector

Stage 1 · Concept: scheduler behavior, `sched_getcpu()`

## 1. Objective

Project 01 found that an unpinned worker on an idle 24-core machine mostly
stays put, but its 20-rep "migration hunt" did see a handful of migrations,
all (by manual inspection) between SMT siblings of the same physical core.
This project turns that side observation into the primary subject: build a
dedicated migration detector that (a) classifies every migration as
same-core (SMT sibling) vs cross-core automatically, using real kernel
topology data instead of eyeballing a CPU list, and (b) tests whether the
detector's own polling granularity changes how many migrations it reports —
i.e., can a coarse-interval detector (like Project 01's) miss real
migrations that a finer one would catch?

## 2. Environment

Base: `docs/ENVIRONMENT.md` (`wsl2`). No deltas — machine was otherwise idle
during all runs (AMD Ryzen 9 7900X, 24 logical CPUs / 12 physical cores, 1
NUMA node, 2-way SMT).

## 3. What I tested

Four executables:

```
03_cpu_migration/src/
├── cpu_topology.hpp          shared core_id lookup (not an experiment)
├── exp1_topology_map.cpp     Experiment 1 — ground truth: which logical CPUs share a physical core
├── exp2_coarse_detector.cpp  Experiment 2 — baseline: migration detector, coarse poll interval
├── exp3_fine_detector.cpp    Experiment 3 — modified: same detector, 1000x finer poll interval
└── exp4_compare.cpp          Experiment 4 — both interval trackers in ONE run, same migrations
```

Like `02_latency_timer`'s `latency_stats.hpp`, `cpu_topology.hpp` is shared
via plain `#include` across every binary that needs it — it is the fixed
measurement apparatus this project builds (SMT-sibling classification via
`/sys/devices/system/cpu/cpuN/topology/core_id`), not part of the
experimental progression being diffed.

```bash
cmake --build build
./build/03_cpu_migration/03_exp1_topology_map
./build/03_cpu_migration/03_exp2_coarse_detector
./build/03_cpu_migration/03_exp3_fine_detector
./build/03_cpu_migration/03_exp4_compare
```

`diff src/exp2_coarse_detector.cpp src/exp3_fine_detector.cpp` shows the one
change that matters: `kCheckInterval`, 1,000,000 vs 1,000.

The workload is Project 01's: 2,000,000,000 iterations of `sink += i` on a
`volatile` accumulator in a single unpinned thread, so the compiler cannot
optimize the loop away and the elapsed time is directly comparable to
Project 01's numbers on the same machine.

Reproducing the measurements (`benchmark/run.sh <binary> <reps> <label>`):

```bash
benchmark/run.sh 03_exp1_topology_map    1  topology
benchmark/run.sh 03_exp2_coarse_detector 20 coarse
benchmark/run.sh 03_exp3_fine_detector   20 fine
benchmark/run.sh 03_exp4_compare         30 compare
```

## 4. Baseline configuration

`03_exp2_coarse_detector` — polls `sched_getcpu()` every 1,000,000
iterations, the exact interval Project 01 used. Every detected change is
classified same-core vs cross-core via `cpu_topology.hpp`.

## 5. Modified configuration

`03_exp3_fine_detector` — identical, except the poll interval is 1,000
iterations, 1000x finer.

Hypothesis (written *before* measuring): a coarse interval can miss a
migrate-and-return that happens entirely between two checkpoints — the
thread leaves CPU A, runs briefly on CPU B, and is back on A (or a third
CPU) before the next sample, so the coarse detector sees nothing. The fine
detector samples 1000x more often and should report a migration count at
least as high as, plausibly higher than, the coarse detector's. Because the
modulo-branch that guards each `sched_getcpu()` call runs every iteration
regardless of the interval's value (only how often the cheap, non-trapping
vDSO call actually fires differs), the two detectors are expected to cost
about the same wall-clock time — so this stays a single-variable comparison
of detection method, not detection cost.

Experiment 4 (`03_exp4_compare`) removes a confound the above pair can't:
Experiments 2 and 3 run on *different* executions, so a run-to-run
difference in how many migrations actually happened (idle-machine
migrations are rare and not reproducible on demand, per Project 01) could
masquerade as a detection-method difference. Experiment 4 runs both a
coarse and a fine tracker inside the **same** execution, both fed the same
`sched_getcpu()` reads, so any difference in what they report can only come
from sampling granularity, not from different underlying events.

## 6. Measurements

Warmup: **0** — a single `sched_getcpu()` call at thread start establishes
the starting CPU; there is nothing to warm up for a migration count, and
Project 01 already established the workload itself needs no warmup at this
iteration count. Iterations: 2,000,000,000 per run (Project 01's workload,
unchanged, for comparability). Repetitions: 20 for Experiments 2 and 3 (the
same count Project 01 used for its migration hunt), 30 for Experiment 4
(migrations are rare events, so more repetitions were used for the
decisive same-run comparison). Experiment 1 is deterministic topology
enumeration, run once.

## 7. Results

### Topology (Experiment 1)

12 physical cores, each exactly 2 logical CPUs, sequential and contiguous:
`core N → [2N, 2N+1]` for N = 0..11 (e.g. core 3 → CPUs 6-7, core 11 → CPUs
22-23) — a cleaner pattern than Project 01 needed to assume from 3 manually
checked pairs.

### Baseline vs. modified, separate runs (20 reps each)

| Metric | Baseline (exp2, coarse ×1,000,000) | Modified (exp3, fine ×1,000) |
|---|---|---|
| Total migrations (20 reps) | 2 | 1 |
| Same-core (SMT) | 1 | 1 |
| Cross-core | 1 | 0 |
| Mean elapsed | 770.0 ms | 780.9 ms |

### Same-run comparison (Experiment 4, 30 reps)

| Metric | Coarse tracker | Fine tracker |
|---|---|---|
| Total migrations (30 reps) | 11 | 11 |
| Same-core (SMT) | 11 | 11 |
| Cross-core | 0 | 0 |
| Reps with a discrepancy between trackers | 0 / 30 | 0 / 30 |
| Mean elapsed | 782.8 ms (both trackers active) | |

Raw output: `results/2026-09-11_topology.txt`,
`results/2026-09-11_coarse.txt`, `results/2026-09-11_fine.txt`,
`results/2026-09-11_compare.txt`

## 8. Why the results happened

**The hypothesis was not confirmed: in 30 same-run comparisons, the fine
tracker never reported more migrations than the coarse tracker — they
matched exactly every time**, including the 9 reps where real migrations
occurred (counts of 1 or 2). The mechanism the hypothesis predicted
(migrate-and-return between two coarse checkpoints) requires a migration
that is both brief and reversed within a ~1,000,000-iteration (~0.4 ms)
window; on this idle machine, the CFS load balancer's migrations appear to
be durable moves — once the thread lands on a new CPU it stays there,
rather than bouncing back — so a coarse enough sample point still catches
every one, because every coarse checkpoint is itself a strict subset of the
fine checkpoints (1,000,000 is a multiple of 1,000) and the migrated-to CPU
value persists long enough to be visible at the next coarse checkpoint too.
This is a genuine negative result, not an inconclusive one: it says
something real about migration duration on this machine, not merely that
the experiment failed to detect a difference.

**Experiments 2 and 3, compared across separate runs, appear to show the
*opposite* of the hypothesis (baseline found 2 migrations, modified found
only 1)** — but Experiment 4's controlled, same-run design shows this is
run-to-run variance in a rare stochastic event, not a detection-method
effect: with only ~0.37 migrations per run on average (11 across 30 reps),
two independently-drawn 20-rep samples can easily differ by one event in
either direction. This is exactly why Experiment 4 was necessary and is the
trustworthy number; Experiments 2 vs. 3's raw totals should not be read as
"coarse polling out-detected fine polling."

**Migrations skew heavily toward same-core (SMT sibling) moves but are not
exclusively that** — refining Project 01's finding. Combining all three
detector runs (70 total 2e9-iteration executions: 20 + 20 + 30), 13 of 14
observed migrations (92.9%) were same-core, matching Project 01's smaller
sample where *all* observed migrations were same-core. But Experiment 2
recorded one genuine cross-core migration (CPU 0 → CPU 4, i.e. core 0 →
core 2), which a 20-rep sample the size of Project 01's could easily have
missed entirely. The CFS load balancer prefers keeping a thread on an SMT
sibling of its current core when rebalancing (cheaper: shared L1/L2, no
cache/NUMA-adjacent cost), but that preference is not absolute — the exact
policy and how it weighs SMT-locality against other cores' load is a Linux
kernel scheduler internal, not something this experiment can observe
directly, only its outcome.

**Coarse and fine detectors cost almost the same wall-clock time (770.0 ms
vs 780.9 ms across separate 20-rep runs; both trackers together in
Experiment 4 cost 782.8 ms), confirming the architectural prediction in
Section 5.** The per-iteration cost is dominated by the modulo/branch check
that runs on *every* iteration regardless of the interval value (both are
compile-time `constexpr` divisors, so the compiler strength-reduces the
division to a multiply-shift of similar cost either way); the frequency
difference only changes how often the cheap `sched_getcpu()` vDSO read
itself fires (2,000 times vs. 2,000,000 times over 2 billion iterations),
and that call is cheap enough (no kernel trap) that even a 1000x increase
in call count is lost in the noise. This means finer-grained migration
detection is close to free here — unlike Project 02's `now()` instrumentation,
which measurably inflated its own results.

## 9. What I learned

- A migration detector's own polling interval is a real methodological
  variable — a coarse interval can, in principle, miss transient
  migrate-and-return events — but whether that risk actually materializes
  depends on how the underlying migrations behave (durable vs. transient),
  which has to be measured, not assumed. Here it did not materialize.
- Comparing a "before" and "after" configuration across *separate* runs is
  unreliable when the phenomenon itself is a rare, run-to-run stochastic
  event; the controlled same-run design (Experiment 4) is what makes the
  comparison trustworthy, not just running more repetitions of the
  separate versions.
- `/sys/devices/system/cpu/cpuN/topology/core_id` gives an exact,
  programmatic SMT-sibling classification directly from the kernel — no
  need to parse `thread_siblings_list` ranges or check pairs by hand, as
  Project 01 did manually for 3 pairs.
- `sched_getcpu()` is a cheap vDSO read (no syscall trap), so increasing
  how often a program checks its CPU by 1000x is not proportionally
  expensive — the cost of *this kind* of instrumentation is dominated by
  the branch/modulo guarding the call, not the call itself.
- A single-sample "always true" claim (Project 01's "not one migration
  crossed to a different physical core") should be treated as a
  small-sample observation, not a rule — a larger combined sample here
  found a genuine counterexample the smaller sample could easily have
  missed.
- A negative result (hypothesis not confirmed) is still a result worth
  explaining mechanistically, not a failed experiment.

## 10. Limitations

- Only 14 real migration events were observed across 70 total runs (2e9
  iterations, ~780 ms each) — enough to see a pattern (durable moves,
  SMT-locality skew), not enough to bound migration rate or the same-core
  fraction precisely; a 95%-confidence interval on 13/14 same-core is wide.
- The negative result in Section 8 (coarse never missed what fine caught)
  is specific to this idle 24-core machine's migration behavior; a busier
  machine, a different kernel scheduler version, or deliberately induced
  contention could produce more transient migrate-and-return events that
  *would* separate the two detectors. This project did not test contended
  conditions — only single-threaded, otherwise-idle-machine behavior, same
  as Projects 01 and 02.
- `core_id` grouping assumes a simple 2-way SMT topology (confirmed for
  this machine in Experiment 1); a machine with >2-way SMT or asymmetric
  core types (e.g. P-cores/E-cores) would need `cpu_topology.hpp` extended
  before its same-core classification could be trusted there.
- Single machine, single session, WSL2's virtualized kernel and scheduler
  are not a native Linux host's — migration behavior under WSL2's
  virtualization layer may not match bare-metal CFS behavior.
- `kCheckInterval` values (1,000,000 / 1,000) were chosen to mirror Project
  01 and to give a 1000x contrast; they were not swept, so the exact
  interval at which a coarse detector would start missing real events
  (if this machine's migrations were ever transient) is unknown.

## 11. Next experiment

Project 04 — Cache Latency Lab moves from *where* a thread runs to *what
memory access costs it* once there. A good follow-up specific to this
project (not required by the roadmap): deliberately induce contention (more
runnable CPU-bound threads than cores) to test whether Section 8's negative
result — durable, not transient, migrations — still holds when the
scheduler is under real pressure to rebalance.

---

### Questions I can answer
- [x] What does `sched_getcpu()` actually report, and how is it different
      from CPU affinity?
- [x] How do you programmatically determine whether two logical CPUs are
      SMT siblings of the same physical core?
- [x] Why can a migration detector's own polling interval change what it
      reports, in principle — and why did that risk not materialize here?
- [x] Why is comparing a baseline and a modified configuration across
      separate runs unreliable when the underlying event is rare and
      stochastic, and what design removes that confound?
- [x] Why does increasing `sched_getcpu()` polling frequency by 1000x not
      cost 1000x more wall-clock time?
- [x] What does it mean for a negative result (hypothesis not confirmed) to
      still be a genuine finding rather than a failed experiment?
- [x] Why should a claim based on a small sample ("all migrations were
      same-core") be treated cautiously even when it holds in that sample?
