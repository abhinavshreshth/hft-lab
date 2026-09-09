// Experiment 2 — Detect the Current CPU  (docx section 9)
//
// Experiment 1 plus instrumentation: which CPU is the worker on, does that
// CPU change while it runs, and how many times?
//
// Diff this against exp1_unpinned.cpp to see exactly what was added.
// Still no affinity — that is Experiment 3.

#include <iostream>
#include <thread>
#include <chrono>
#include <sched.h>

constexpr long kIterations = 2'000'000'000;

// sched_getcpu() costs far more than one "sink += i" (it is a vDSO call, not
// a register add). Calling it every iteration would measure call overhead
// instead of the workload. Every 1,000,000 iterations still catches a move.
constexpr long kCheckInterval = 1'000'000;

void worker() {
    using clock = std::chrono::steady_clock;

    // sched_getcpu() reports the CPU we are on RIGHT NOW. It is a snapshot,
    // not a promise: Linux may move this thread on the very next instruction.
    int start_cpu = sched_getcpu();
    int last_cpu = start_cpu;
    long migrations = 0;

    volatile long sink = 0;

    auto t0 = clock::now();
    for (long i = 0; i < kIterations; ++i) {
        sink += i;

        if (i % kCheckInterval == 0) {
            int current_cpu = sched_getcpu();
            if (current_cpu != last_cpu) {
                migrations++;          // the thread moved to another CPU
                last_cpu = current_cpu;
            }
        }
    }
    auto t1 = clock::now();

    double elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "start CPU:  " << start_cpu << "\n";
    std::cout << "final CPU:  " << last_cpu << "\n";
    std::cout << "migrations: " << migrations << "\n";
    std::cout << "iterations: " << kIterations << "\n";
    std::cout << "elapsed:    " << elapsed_ms << " ms\n";
    std::cout << "sink:       " << sink << "\n";
}

int main() {
    std::cout << "=== Experiment 2: detect current CPU (still unpinned) ===\n";
    std::thread t(worker);
    t.join();
    return 0;
}
