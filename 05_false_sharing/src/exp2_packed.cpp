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

#include <cstdlib>
#include <iomanip>
#include <iostream>

#include "false_sharing.hpp"

constexpr std::uint64_t kIterations = 50'000'000;

int main(int argc, char** argv) {
    int cpu_a = argc > 2 ? std::atoi(argv[1]) : kDefaultCpuA;
    int cpu_b = argc > 2 ? std::atoi(argv[2]) : kDefaultCpuB;

    PackedLayout layout;

    std::cout << "=== Experiment 2: " << layout.layout_name() << " counters, 2 threads ===\n";
    std::cout << "cpus " << cpu_a << " (core " << physical_core_of(cpu_a) << ") and " << cpu_b << " (core "
              << physical_core_of(cpu_b) << "), " << kIterations << " increments per thread, " << kTrials
              << " trials\n";
    std::cout << "counter lines: " << line_of(layout.counter(0)) << ", " << line_of(layout.counter(1))
              << (line_of(layout.counter(0)) == line_of(layout.counter(1)) ? "  (SAME line)\n" : "  (different lines)\n");

    auto m = measure(layout, {cpu_a, cpu_b}, kIterations);

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "slowest-thread ns/increment  min " << m.stats.min << "  p50 " << m.stats.p50 << "  max "
              << m.stats.max << "\n";
    std::cout << "checks: " << (m.ok ? "ok" : "FAILED (count or pinning)") << "\n";
    return m.ok ? 0 : 1;
}
