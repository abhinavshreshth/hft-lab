// Experiment 2 — Coarse-Interval Migration Detector (Baseline)
//
// The measurement method Project 01 used as a side effect (exp2_detect_cpu,
// exp4_compare): poll sched_getcpu() every kCheckInterval iterations. Here
// it is the primary subject, not a side check, and every detected
// migration is classified same-core (SMT sibling) vs cross-core using
// cpu_topology.hpp and Experiment 1's ground truth.
//
// Diff this against exp3_fine_detector.cpp: the only change is
// kCheckInterval. Both use the same modulo-branch-every-iteration shape, so
// their runtime cost is expected to be nearly identical — isolating
// "how many migrations did this method see" from "how much did checking
// cost", which is exactly the variable Experiment 4 compares head to head.

#include <iostream>
#include <thread>
#include <chrono>
#include <sched.h>

#include "cpu_topology.hpp"

constexpr long kIterations = 2'000'000'000;
constexpr long kCheckInterval = 1'000'000;  // Project 01's interval

struct Result {
    int start_cpu = 0;
    int final_cpu = 0;
    long migrations_total = 0;
    long migrations_same_core = 0;
    long migrations_cross_core = 0;
    double elapsed_ms = 0.0;
};

Result run_worker(const hft::CpuTopology& topo) {
    using clock = std::chrono::steady_clock;

    Result r;
    int start_cpu = sched_getcpu();
    int last_cpu = start_cpu;
    r.start_cpu = start_cpu;

    volatile long sink = 0;

    auto t0 = clock::now();
    for (long i = 0; i < kIterations; ++i) {
        sink += i;

        if (i % kCheckInterval == 0) {
            int current_cpu = sched_getcpu();
            if (current_cpu != last_cpu) {
                r.migrations_total++;
                if (topo.same_core(last_cpu, current_cpu)) {
                    r.migrations_same_core++;
                } else {
                    r.migrations_cross_core++;
                }
                last_cpu = current_cpu;
            }
        }
    }
    auto t1 = clock::now();

    r.final_cpu = last_cpu;
    r.elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    (void)sink;
    return r;
}

int main() {
    std::cout << "=== Experiment 2: coarse-interval detector (baseline) ===\n";

    hft::CpuTopology topo;
    Result r;
    std::thread t([&] { r = run_worker(topo); });
    t.join();

    std::cout << "check interval:     every " << kCheckInterval << " iterations\n";
    std::cout << "start CPU:          " << r.start_cpu << "\n";
    std::cout << "final CPU:          " << r.final_cpu << "\n";
    std::cout << "migrations total:   " << r.migrations_total << "\n";
    std::cout << "  same-core (SMT):  " << r.migrations_same_core << "\n";
    std::cout << "  cross-core:       " << r.migrations_cross_core << "\n";
    std::cout << "iterations:         " << kIterations << "\n";
    std::cout << "elapsed:            " << r.elapsed_ms << " ms\n";

    return 0;
}
