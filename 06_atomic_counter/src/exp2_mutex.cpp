// Experiment 2 — Baseline: two threads sharing one counter behind a std::mutex.
//
// Two threads pinned to two distinct physical cores, both incrementing the
// SAME counter — true sharing this time, unlike Project 05 — so every
// increment must be protected or updates are lost (Experiment 1's negative
// control). The conventional protection is a mutex: lock, ++, unlock.
//
// Diff this against exp3_atomic.cpp: the only change is which Counter is
// constructed.
//
// Usage: ./06_exp2_mutex [cpuA cpuB]      (default CPUs 2 and 4)

#include <cstdlib>
#include <iomanip>
#include <iostream>

#include "atomic_counter.hpp"

constexpr std::uint64_t kIterations = 5'000'000;

int main(int argc, char** argv) {
    int cpu_a = argc > 2 ? std::atoi(argv[1]) : kDefaultCpuA;
    int cpu_b = argc > 2 ? std::atoi(argv[2]) : kDefaultCpuB;

    MutexCounter counter;

    std::cout << "=== Experiment 2: " << counter.kName << ", 1 shared counter, 2 threads ===\n";
    std::cout << "cpus " << cpu_a << " (core " << physical_core_of(cpu_a) << ") and " << cpu_b << " (core "
              << physical_core_of(cpu_b) << "), " << kIterations << " increments per thread, " << kTrials
              << " trials\n";

    auto m = measure(shared_by(counter, 2), {cpu_a, cpu_b}, kIterations);

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "slowest-thread ns/increment  min " << m.slowest.min << "  p50 " << m.slowest.p50 << "  max "
              << m.slowest.max << "\n";
    std::cout << "fastest-thread ns/increment  min " << m.fastest.min << "  p50 " << m.fastest.p50 << "  max "
              << m.fastest.max << "\n";
    std::cout << "aggregate M increments/s     min " << m.aggregate_mops.min << "  p50 " << m.aggregate_mops.p50
              << "  max " << m.aggregate_mops.max << "\n";
    std::cout << std::setprecision(1);
    std::cout << "context switches per trial   voluntary " << m.voluntary_switches_per_trial << "  involuntary "
              << m.involuntary_switches_per_trial << "\n";
    std::cout << "kernel share of thread CPU   " << 100.0 * m.kernel_fraction << "%\n";
    std::cout << "checks: " << (m.ok() ? "ok" : "FAILED (count or pinning)") << "\n";
    return m.ok() ? 0 : 1;
}
