// Experiment 2 — Baseline: two threads, counters packed into one cache line.
//
// Two threads pinned to two distinct physical cores. Each increments ONLY its
// own counter — no shared data, no locks, no atomics — but the two counters
// are adjacent uint64_t in one 64-byte line (PackedLayout: what
// `std::uint64_t counters[N];` gives you by default).
//
// Diff this against exp3_padded.cpp: the only change is which CounterLayout
// is constructed.
//
// Usage: ./05_exp2_packed [cpuA cpuB]      (default CPUs 2 and 4)

#include "lab/cpu_placement.hpp"
#include "lab/report.hpp"
#include "lab/trials.hpp"

using namespace false_sharing;

constexpr std::uint64_t kIterations = 50'000'000;

int main(int argc, char** argv) {
    std::vector<int> cpus = cpus_from_args(argc, argv, {kDefaultCpuA, kDefaultCpuB});

    PackedLayout layout;

    Measurement m = measure(layout, cpus, kIterations);

    report::two_thread_run("Experiment 2", layout, cpus, kIterations, m);
    return m.ok ? 0 : 1;
}
