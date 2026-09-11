// Experiment 4 — Compare Coarse vs Fine, Same Run
//
// Experiments 2 and 3 compare coarse vs fine detection across *different*
// runs, so a run-to-run difference in actual migrations (idle-machine
// migrations are rare and not reproducible on demand, per Project 01) could
// masquerade as a detection-method difference. This experiment removes that
// confound: one worker thread, one execution of the identical workload,
// checked by BOTH a coarse-interval tracker and a fine-interval tracker at
// the same time, so both detectors are exposed to exactly the same set of
// real migration events. Any difference in what they report can only come
// from sampling granularity.

#include <iostream>
#include <thread>
#include <chrono>
#include <sched.h>

#include "cpu_topology.hpp"

constexpr long kIterations = 2'000'000'000;
constexpr long kCoarseInterval = 1'000'000;  // Experiment 2's interval
constexpr long kFineInterval = 1'000;        // Experiment 3's interval

struct Tally {
    int last_cpu = 0;
    long total = 0;
    long same_core = 0;
    long cross_core = 0;
};

void record(Tally& t, int current_cpu, const hft::CpuTopology& topo) {
    if (current_cpu != t.last_cpu) {
        t.total++;
        if (topo.same_core(t.last_cpu, current_cpu)) {
            t.same_core++;
        } else {
            t.cross_core++;
        }
        t.last_cpu = current_cpu;
    }
}

struct Result {
    int start_cpu = 0;
    Tally coarse;
    Tally fine;
    double elapsed_ms = 0.0;
};

Result run_worker(const hft::CpuTopology& topo) {
    using clock = std::chrono::steady_clock;

    Result r;
    int start_cpu = sched_getcpu();
    r.start_cpu = start_cpu;
    r.coarse.last_cpu = start_cpu;
    r.fine.last_cpu = start_cpu;

    volatile long sink = 0;

    auto t0 = clock::now();
    for (long i = 0; i < kIterations; ++i) {
        sink += i;

        if (i % kFineInterval == 0) {
            int current_cpu = sched_getcpu();
            record(r.fine, current_cpu, topo);

            // The coarse tracker only looks at every kCoarseInterval-th
            // sample point, reusing the same sched_getcpu() call so it is
            // exposed to the exact same underlying CPU value the fine
            // tracker saw at that instant.
            if (i % kCoarseInterval == 0) {
                record(r.coarse, current_cpu, topo);
            }
        }
    }
    auto t1 = clock::now();

    r.elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    (void)sink;
    return r;
}

int main() {
    std::cout << "=== Experiment 4: coarse vs fine, same run ===\n";

    hft::CpuTopology topo;
    Result r;
    std::thread t([&] { r = run_worker(topo); });
    t.join();

    std::cout << "start CPU:                " << r.start_cpu << "\n";
    std::cout << "coarse (every " << kCoarseInterval << "): "
              << r.coarse.total << " migrations"
              << "  (same-core " << r.coarse.same_core
              << ", cross-core " << r.coarse.cross_core << ")\n";
    std::cout << "fine   (every " << kFineInterval << "):     "
              << r.fine.total << " migrations"
              << "  (same-core " << r.fine.same_core
              << ", cross-core " << r.fine.cross_core << ")\n";
    std::cout << "iterations:               " << kIterations << "\n";
    std::cout << "elapsed:                  " << r.elapsed_ms << " ms\n";

    return 0;
}
