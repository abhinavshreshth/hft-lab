// Experiment 3 — Fine-Interval Migration Detector (Modified)
//
// Identical to exp2_coarse_detector.cpp except kCheckInterval, 1000x
// smaller. Diff the two files to confirm that is the only change.
//
// Hypothesis (written *before* measuring): a coarse interval can miss a
// migrate-and-return that happens entirely between two checkpoints — the
// thread leaves cpu A, runs briefly on cpu B, and is back on A (or a third
// cpu) by the next sample, so the coarse detector sees no change at all.
// The fine detector samples 1000x more often and should therefore report a
// migration count at least as high as, and plausibly higher than, the
// coarse detector's — while costing roughly the same wall-clock time,
// because the per-iteration modulo/branch runs regardless of the divisor
// (see exp2's header comment); only the number of times the cheap
// sched_getcpu() call actually fires changes, and that call itself is a
// cheap vDSO read, not a trapping syscall.

#include <iostream>
#include <thread>
#include <chrono>
#include <sched.h>

#include "cpu_topology.hpp"

constexpr long kIterations = 2'000'000'000;
constexpr long kCheckInterval = 1'000;  // 1000x finer than Experiment 2

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
    std::cout << "=== Experiment 3: fine-interval detector (modified) ===\n";

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
