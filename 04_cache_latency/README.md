# Project 04 — Cache Latency Lab

Stage 1 · Concept: cache vs RAM, access patterns

## 1. Objective

Project 03 measured *where* a thread runs. This project measures *what a
memory access costs it* once it lands somewhere — specifically, whether the
memory hierarchy's latency (L1 → L2 → L3 → RAM, per `docs/ENVIRONMENT.md`'s
32K/1M/32M cache sizes) is actually visible to a program, or whether the
hardware prefetcher hides it. The question: does per-access latency depend
only on working-set size, or does it depend on *access pattern* — and if
the latter, by how much?

## 2. Environment

Base: `docs/ENVIRONMENT.md` (`wsl2`). No deltas — AMD Ryzen 9 7900X, L1d
32K, L2 1M, L3 32M (shared), machine otherwise idle during all runs.

## 3. What I tested

Four executables, all built on one shared apparatus:

```
04_cache_latency/src/
├── cache_lab.hpp             shared apparatus (not an experiment)
├── exp1_chain_sanity.cpp     Experiment 1 — validate the apparatus itself
├── exp2_sequential_sweep.cpp Experiment 2 — baseline: sequential-order chase
├── exp3_random_sweep.cpp     Experiment 3 — modified: random-order chase
└── exp4_compare.cpp          Experiment 4 — both patterns in ONE run
```

`cache_lab.hpp` implements the standard pointer-chasing latency benchmark
(the same technique behind tools like `lat_mem_rd`): allocate `N` 64-byte
`Node`s (one per cache line, `alignas(64)` forces `sizeof(Node) == 64`), so
a buffer of `N` nodes occupies exactly `N` cache lines. Chain construction
and chain traversal are split into two classes, the same way Project 03's
`CpuTopology` depends on an injected `CoreIdReader` rather than reading
sysfs itself:

- **`ChainBuilder`** — an abstract interface (`build(n)`, `pattern_name()`)
  for laying out a chain's `next` fields. Two concrete builders implement
  it: `SequentialChainBuilder` (`next[i] = (i + 1) % n` — cache lines
  visited in increasing address order) and `RandomChainBuilder` (a single
  `N`-node cycle built with **Sattolo's algorithm** — not Fisher-Yates,
  which can produce several disjoint short cycles and let the chase settle
  into a small, easily-cached loop instead of touching all `N` cache
  lines; Sattolo guarantees one cycle covering every node, in an order a
  hardware prefetcher cannot predict).
- **`PointerChaser`** — the fixed apparatus that times a chase over
  whichever chain it is handed. It depends only on the `ChainBuilder`
  interface, never on a concrete pattern, so adding a third access pattern
  later (Project 04's own "next experiment," Section 11) means writing one
  new `ChainBuilder` subclass, not touching `PointerChaser` or either
  existing experiment.

Experiments 2 and 3 differ by exactly one line — which concrete
`ChainBuilder` is constructed — because the sweep loop in each only calls
the interface. Experiment 4 holds both builders behind `const ChainBuilder*`
in an alternating-order array for the same reason: the loop that actually
runs the chase never branches on which concrete pattern it is holding.

The chase itself is `p = nodes[p].next` in a loop: each step's memory
address depends on the *value* loaded by the previous step, so the CPU
cannot issue the next load until the current one returns. This is what
makes it a latency measurement rather than a bandwidth measurement — there
is nothing here for out-of-order execution to overlap.

The sweep covers 12 sizes chosen to straddle every boundary in
`docs/ENVIRONMENT.md`'s cache hierarchy: 4/16/32 KiB (L1), 64/256 KiB/1 MiB
(L2), 2/8/16/32 MiB (L3), and 64/128 MiB (solidly beyond L3, into RAM).

```bash
cmake --build build
./build/04_cache_latency/04_exp1_chain_sanity
./build/04_cache_latency/04_exp2_sequential_sweep
./build/04_cache_latency/04_exp3_random_sweep
./build/04_cache_latency/04_exp4_compare
```

`diff src/exp2_sequential_sweep.cpp src/exp3_random_sweep.cpp` shows the one
change that matters: which `build_*_chain()` function is called.

Reproducing the measurements (`benchmark/run.sh <binary> <reps> <label>`):

```bash
benchmark/run.sh 04_exp1_chain_sanity      5 sanity
benchmark/run.sh 04_exp2_sequential_sweep  5 sequential
benchmark/run.sh 04_exp3_random_sweep      5 random
benchmark/run.sh 04_exp4_compare           5 compare
```

## 4. Baseline configuration

`04_exp2_sequential_sweep` — chases the sequential-order chain at every
size in the sweep. `next[i] = (i + 1) % n` is a constant unit-stride
pattern in cache-line space; this is exactly the access shape a hardware
prefetcher is designed to detect and run ahead of.

## 5. Modified configuration

`04_exp3_random_sweep` — identical harness, except the chain is a single
random Sattolo cycle instead of a sequential one.

Hypothesis (written *before* measuring): a prefetcher can only exploit a
*predictable* address stream. The random chain makes the next cache line
unknowable until the current node's value is actually loaded, so once the
working set exceeds whatever tier can hold it, every step should show that
tier's real miss latency — while the sequential sweep should stay low and
roughly flat across every size, because the prefetcher hides the latency
regardless of which tier the data physically lives in. Concretely:
random-order `ns/access` should climb in a staircase at the L1→L2 (32
KiB), L2→L3 (1 MiB), and L3→RAM (32 MiB) boundaries; sequential-order
`ns/access` should not.

Experiment 4 (`04_exp4_compare`) removes a confound the separate-run pair
can't: Experiments 2 and 3 run in different executions, so a difference
between their numbers could in principle reflect run-to-run machine state
(frequency scaling, other processes) rather than access order itself.
Experiment 4 chases both chains, for every size, inside one execution, and
alternates which pattern runs first per size so that whichever pattern gets
any frequency-ramp head start cannot systematically favor one side across
the whole sweep.

## 6. Measurements

Warmup: `4 × n` chase steps per size (untimed) before every measured run —
enough to bring the working set to whatever residency it will reach (full
cache residency if it fits a tier, otherwise just page-fault-in for sizes
that don't). Iterations: 3,000,000 measured chase steps per size, fixed
across all 12 sizes for comparability. Repetitions: 5 full sweeps per
experiment. `04_exp1_chain_sanity` validates, for every size in the sweep,
that `RandomChainBuilder` actually produces a single `N`-cycle (not
several shorter ones) before any of its latency numbers are trusted, and
checks that sequential and random order cost about the same at the
smallest (L1-resident) size — if they didn't, the harness itself, not the
cache hierarchy, would be the source of any measured difference.

Outlier handling: the raw per-size numbers are reported as means across 5
repetitions with the observed min/max noted where the spread is large
enough to matter (Section 7); no repetitions were discarded.

A correctness note on the benchmark itself: the first version of every
sweep executable reported `0.000 ns/access` at every size, because
`PointerChaser::chase()`'s returned `final_index` was never consumed by the
caller — with
nothing observing the chase loop's only output, the compiler proved the
entire warmup and measured loop dead and deleted it (`-O2`), leaving only
two `steady_clock::now()` calls around empty code. Fixed by printing
`final_index` (and, in Experiment 4, XOR-folding both chains' final indices
into a value printed at the end) so the loads are observably used. This is
exactly the `docs/MEASUREMENT.md` "do not let the compiler delete your
benchmark" warning, caught by a `0.000`/near-instant run rather than by
inspection — an instance of the warning worth recording, not just citing.

## 7. Results

### Same-run comparison (Experiment 4, mean of 5 reps)

| Size | seq ns/access | rnd ns/access | rnd/seq ratio |
|---|---|---|---|
| 4 KiB (L1) | 0.961 | 0.932 | 0.97 |
| 16 KiB (L1) | 0.926 | 0.936 | 1.01 |
| 32 KiB (L1/L2 boundary) | 0.928 | 1.021 | 1.10 |
| 64 KiB (L2) | 1.012 | 2.790 | 2.76 |
| 256 KiB (L2) | 0.968 | 2.801 | 2.89 |
| 1 MiB (L2/L3 boundary) | 1.027 | 6.073 | 5.91 |
| 2 MiB (L3) | 0.993 | 9.228 | 9.31 |
| 8 MiB (L3) | 0.979 | 10.808 | 11.05 |
| 16 MiB (L3) | 0.981 | 29.000* | 29.58* |
| 32 MiB (L3/RAM boundary) | 1.208 | 94.842 | 78.99 |
| 64 MiB (RAM) | 1.412 | 101.417 | 72.31 |
| 128 MiB (RAM) | 1.316 | 112.113 | 85.63 |

\* 16 MiB shows high rep-to-rep spread (14.5–55.4 ns) — see Section 8.

### Separate runs (Experiments 2 and 3, mean of 5 reps each)

| Size | seq ns/access | rnd ns/access |
|---|---|---|
| 4 KiB | 0.940 | 0.959 |
| 16 KiB | 0.930 | 0.942 |
| 32 KiB | 0.939 | 1.018 |
| 64 KiB | 1.028 | 2.832 |
| 256 KiB | 0.979 | 2.835 |
| 1 MiB | 1.028 | 6.353 |
| 2 MiB | 0.988 | 9.347 |
| 8 MiB | 0.984 | 11.274 |
| 16 MiB | 0.988 | 46.636* |
| 32 MiB | 1.286 | 90.910 |
| 64 MiB | 1.239 | 103.954 |
| 128 MiB | 1.226 | 110.005 |

Raw output: `results/2026-09-13_sanity.txt`, `results/2026-09-13_sequential.txt`,
`results/2026-09-13_random.txt`, `results/2026-09-13_compare.txt`

## 8. Why the results happened

**The hypothesis was confirmed, cleanly.** Sequential-order `ns/access`
stays within 0.93–1.41 ns across the *entire* sweep, from 4 KiB to 128 MiB
— it does not know, or care, that 128 MiB is 4x larger than the L3 cache.
Random-order `ns/access` instead climbs in the predicted staircase: ~0.9-1
ns while inside L1 (≤32 KiB), a jump to ~2.8 ns once the set exceeds L1 and
lives in L2 (64-256 KiB), a further jump to ~6-11 ns through the L3 range
(1-8 MiB), and a final jump to ~90-112 ns once the set exceeds L3 entirely
(32-128 MiB) — squarely in DRAM-latency territory. The mechanism: a modern
out-of-order core's hardware prefetcher watches the stream of addresses a
core touches and, on recognizing a constant stride (here, literally +1
cache line every access), issues loads for upcoming lines *before* the
program asks for them, so by the time `nodes[p].next` executes, the line is
already resident — the RAM or L3 latency was paid, but hidden behind
the loads the prefetcher issued earlier, in parallel with other work. A
single-cycle random permutation gives the prefetcher nothing to detect:
the next address is a data value that does not exist until the current
load completes, so every access pays that tier's true, unhidden latency,
serialized one after another because the pointer-chase creates a genuine
data dependency between consecutive loads.

**Both the same-run (Experiment 4) and separate-run (Experiments 2/3)
methodologies agree closely** (e.g. 128 MiB random: 112.1 ns same-run vs.
110.0 ns separate-run) — unlike Project 03, where the phenomenon under
study (rare stochastic migrations) made separate-run comparison
unreliable, cache latency is a deterministic hardware property that does
not depend on which execution measures it, so the same-run design mainly
adds confidence rather than overturning a different conclusion the way it
did in Project 03.

**The 16 MiB size showed unusually high rep-to-rep variance** (14.5-55.4 ns
in Experiment 4's individual reps, 22.6-76.3 ns in Experiment 3's) compared
to every neighboring size, which stayed tight to within ~10% across
repetitions. 16 MiB is exactly half of this machine's 32 MiB *shared* L3 —
still comfortably inside L3 capacity on an idle machine, but close enough
to the boundary that eviction pressure from any other activity sharing the
same L3 (other cores, WSL2's host, background processes) can occasionally
push part of the working set out mid-run, producing an inconsistent mix of
L3 hits and RAM misses from one repetition to the next. Sizes further from
either boundary (2-8 MiB comfortably inside L3, 32+ MiB comfortably past
it) show no such spread, which points at capacity-boundary proximity, not
a general measurement problem, as the cause.

## 9. What I learned

- A hardware prefetcher can make a cache hierarchy's actual latency
  numbers essentially invisible to a sequentially-accessing program — the
  observed cost of touching memory is a property of the *access pattern*,
  not just the *working-set size*, and a benchmark that only varies size
  while holding a prefetcher-friendly pattern fixed will report "everything
  is equally fast" regardless of what the underlying hardware costs.
- Sattolo's algorithm, not Fisher-Yates, is the correct shuffle for
  building a pointer-chase benchmark: Fisher-Yates permits multiple
  disjoint cycles, any one of which could be short enough to stay cache
  resident and quietly invalidate the whole "random order defeats the
  cache" premise. Experiment 1 verifying single-cycle-ness before trusting
  any later number is a case where validating the apparatus is as
  important as running the experiment.
- The `-O2` optimizer's dead-code elimination is not hypothetical: it
  silently deleted this project's entire first-draft measurement loop the
  moment nothing downstream consumed the loop's only output, producing
  `0.000 ns/access` rather than an error. `docs/MEASUREMENT.md`'s "consume
  the result" rule is a real, load-bearing requirement, not defensive
  boilerplate.
- A dependent pointer-chase (each load's address depends on the previous
  load's value) is what turns a memory benchmark into a *latency*
  measurement — a loop reading independent addresses would let the CPU's
  out-of-order execution overlap many outstanding misses and measure
  memory-level parallelism / bandwidth instead.
- Capacity boundaries in a cache hierarchy are not knife-edges in practice:
  a working set sized just under a *shared* cache's capacity (16 MiB vs.
  this machine's 32 MiB shared L3) can show meaningfully higher variance
  than sizes comfortably on either side of the boundary, because it is
  more exposed to eviction pressure from anything else using that shared
  resource.

## 10. Limitations

- Single-threaded, otherwise-idle machine, same as Projects 01-03; the L3
  is shared across all 12 physical cores, so a busier machine would likely
  show the 16 MiB boundary effect (Section 8) more strongly, and possibly
  shift the effective L3 boundary lower than 32 MiB.
- `RandomChainBuilder` was constructed with one fixed seed (`kRandomSeed`) in every experiment — these are
  numbers from one particular random permutation per size, not an average
  over many random layouts. A different permutation could land slightly
  differently relative to cache-set associativity conflicts, though the
  tier-to-tier staircase itself is a robust hardware property that would
  not disappear with a different seed.
- WSL2's virtualized kernel: physical-address-to-cache-set mapping and any
  host-side interference are not directly observable or controllable from
  inside the guest, which is a plausible contributor to the 16 MiB
  variance beyond ordinary shared-L3 contention.
- The sweep stops at 128 MiB (4x the 32 MiB L3). This is enough to show a
  clearly flat post-L3 plateau for random order, but not enough to
  characterize RAM latency at very large working sets (TLB miss effects
  from crossing more page-table levels, NUMA effects — moot here, this
  machine has one node — or DRAM row-buffer behavior at larger footprints)
  or to test whether huge pages (`docs/ENVIRONMENT.md`: none reserved on
  this machine) would change the RAM-tier numbers.
- `Node` is 64 bytes with only 8 bytes actually used (the `next` field);
  the other 56 bytes per cache line are untouched padding. This is
  intentional — it isolates "one chase step touches one cache line" as the
  variable under test — but means these numbers characterize *latency to a
  cold cache line*, not the cost of a realistic data structure that packs
  useful payload into the same lines it chases through.

## 11. Next experiment

Project 05 — False Sharing Demo moves from single-threaded access latency
to what happens when *multiple threads* touch data on the same cache
line. A good follow-up specific to this project (not required by the
roadmap): sweep multiple random seeds per size to quantify how much the
16 MiB variance (Section 8) is seed-dependent placement noise vs. genuine
contention, and extend the sweep past 128 MiB to see whether the RAM-tier
plateau (~100-112 ns here) is truly flat or has its own structure at larger
footprints.

---

### Questions I can answer

- [x] Why can a hardware prefetcher make cache-tier latency invisible to a
      program, and what access property defeats it?
- [x] Why does a valid pointer-chase benchmark require a *single* random
      cycle (Sattolo), not just any random permutation (Fisher-Yates)?
- [x] What makes a dependent load chain measure latency instead of
      bandwidth?
- [x] Why did the first version of this benchmark report `0.000
      ns/access`, and what specific rule in `docs/MEASUREMENT.md` predicts
      that failure mode?
- [x] Why did a working-set size close to, but under, the shared L3's
      capacity (16 MiB vs. 32 MiB) show more rep-to-rep variance than sizes
      further from that boundary in either direction?
- [x] Why did the same-run and separate-run comparison methods agree here,
      unlike in Project 03?
