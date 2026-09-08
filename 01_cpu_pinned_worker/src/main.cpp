#include <iostream>
#include <thread>
#include <chrono>
#include <sched.h>

// A thread runs inside the same process as main().
// It shares main's memory, but has its own stack and runs independently.
void worker() {
    using clock = std::chrono::steady_clock;

    // constexpr = fixed at compile time, not a magic number repeated everywhere.
    // long (64-bit here) so this never silently overflows like a 32-bit int
    // could, and so it matches the type of sink/i below with no conversions.
    constexpr long kIterations = 2'000'000'000;  // fixed amount of work

    // sched_getcpu() costs far more than one "sink += i" (it's a vDSO call,
    // not free like a register add). Calling it every iteration would make
    // total runtime measure call overhead, not the workload. Every
    // 1,000,000 iterations is frequent enough to still catch a migration.
    constexpr long kCheckInterval = 1'000'000;

    int start_cpu = sched_getcpu();
    int last_cpu = start_cpu;
    long migrations = 0;

    // volatile forces a real memory read+write every iteration, so the
    // compiler can't collapse this loop into a no-op or a closed-form jump.
    // Without this, the loop body does "nothing" between CPU checks, and
    // -O2 is free to skip straight to the next check — see
    // docs/MEASUREMENT.md: "do not let the compiler delete your benchmark".
    volatile long sink = 0;

    auto t0 = clock::now();

    for (long i = 0; i < kIterations; ++i) {
        sink += i;

        if (i % kCheckInterval == 0) {
            int current_cpu = sched_getcpu();
            if (current_cpu != last_cpu) {
                migrations++;
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
    std::cout << "sink:       " << sink << "\n";  // proves the loop wasn't optimized away
}

int main() {
    std::cout << "main thread running\n";

    // This line creates a real OS thread.
    // From here, main() and worker() run at the same time.
    std::thread t(worker);

    // join() = wait here until the worker thread finishes.
    t.join();

    return 0;
}
