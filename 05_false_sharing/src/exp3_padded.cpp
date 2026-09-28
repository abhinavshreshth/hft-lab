// Experiment 3 — Modified: two threads, each counter padded to its own cache line.
//
// Identical to Experiment 2 — same two pinned threads, same private
// counters, same increment loop — except each counter is an alignas(64)
// PaddedCounter, so the two counters sit in different cache lines and
// neither core's stores ever invalidate the other core's line.
//
// Diff this against exp2_packed.cpp: the only change is which CounterLayout
// is constructed.
//
// Usage: ./05_exp3_padded [cpuA cpuB]      (default CPUs 2 and 4)

#include <cstdlib>
#include <iomanip>
#include <iostream>

#include "false_sharing.hpp"

constexpr std::uint64_t kIterations = 50'000'000;

int main(int argc, char** argv) {
    int cpu_a = argc > 2 ? std::atoi(argv[1]) : kDefaultCpuA;
    int cpu_b = argc > 2 ? std::atoi(argv[2]) : kDefaultCpuB;

    PaddedLayout layout;

    std::cout << "=== Experiment 3: " << layout.layout_name() << " counters, 2 threads ===\n";
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
