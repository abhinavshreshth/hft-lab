// Experiment 1 — Unpinned Worker  (docx section 6)
//
// Goal: run a CPU-bound worker thread with NO affinity configured, and
// establish a baseline for how long the workload takes.
//
// We deliberately do NOT ask which CPU we are on yet — that is Experiment 2.
// Here we only establish: the thread runs, and this is how long it takes.

#include <iostream>
#include <thread>
#include <chrono>

// Fixed amount of work, so "how long did it take" is a meaningful question.
// constexpr = compile-time constant, not a magic number repeated below.
// long is 64-bit here, so the counter and the accumulator can't overflow.
constexpr long kIterations = 2'000'000'000;

void worker() {
    using clock = std::chrono::steady_clock;

    // volatile forces a real memory read+write every iteration, so the
    // compiler cannot delete this loop or fold it into a closed-form
    // formula. See docs/MEASUREMENT.md.
    volatile long sink = 0;

    auto t0 = clock::now();
    for (long i = 0; i < kIterations; ++i) {
        sink += i;
    }
    auto t1 = clock::now();

    double elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "iterations: " << kIterations << "\n";
    std::cout << "elapsed:    " << elapsed_ms << " ms\n";
    std::cout << "sink:       " << sink << "\n";  // proves the loop really ran
}

int main() {
    std::cout << "=== Experiment 1: unpinned worker (baseline) ===\n";

    // Creates a real OS thread. No affinity is set, so Linux places this
    // thread wherever it likes and may move it at any time.
    std::thread t(worker);

    // Wait for the worker to finish before main() returns.
    t.join();

    return 0;
}
