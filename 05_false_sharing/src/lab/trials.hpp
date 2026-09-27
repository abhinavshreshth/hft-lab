// Turning single races into measurements: warmup, repeated trials, and the
// same-run comparison of two layouts.
#pragma once

#include <cstdint>
#include <vector>

#include "counter_layout.hpp"

namespace false_sharing {

// Each configuration is raced kTrials times inside one process; the report is
// the spread of the slowest-thread ns/increment across those trials. With
// ~10 trials p95/p99/p99.9 would all just be the max, so only min/p50/max
// are kept (docs/MEASUREMENT.md's full percentile set applies to per-event
// latencies, which this deliberately does not sample).
constexpr int kTrials = 11;

// One full race per configuration, run and discarded before the timed
// trials: page-faults the counters in, brings the line into the caches
// involved and lets the cores' clocks ramp before anything counts.
constexpr int kWarmupRaces = 1;

struct TrialStats {
    double min = 0.0;
    double p50 = 0.0;
    double max = 0.0;
};

TrialStats summarize_trials(std::vector<double> samples);

struct Measurement {
    TrialStats stats;
    bool ok = true;  // false if any race (warmup included) failed its count or pinning check
};

// kWarmupRaces discarded races, then `trials` timed races of one layout.
Measurement measure(CounterLayout& layout, const std::vector<int>& cpus, std::uint64_t iterations,
                    int trials = kTrials);

// Two layouts measured under the same conditions in one process.
struct Comparison {
    Measurement a;
    Measurement b;
    bool ok() const { return a.ok && b.ok; }
    double p50_ratio() const { return a.stats.p50 / b.stats.p50; }
};

// Measures a then b (or b then a if !a_first). Sweeps flip a_first from
// point to point so neither layout systematically gets the warmer machine.
Comparison measure_both(CounterLayout& a, CounterLayout& b, const std::vector<int>& cpus, std::uint64_t iterations,
                        bool a_first);

// Warms both up, then alternates a and b trial by trial (a first on even
// trials) — the tightest same-run comparison, used by Experiment 4.
Comparison measure_interleaved(CounterLayout& a, CounterLayout& b, const std::vector<int>& cpus,
                               std::uint64_t iterations);

}  // namespace false_sharing
