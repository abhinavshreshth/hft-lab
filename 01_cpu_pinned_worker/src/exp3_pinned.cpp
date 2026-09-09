// Experiment 3 — Pin the Worker  (docx section 11)
//
// Experiment 2 plus CPU affinity: restrict the worker so it is ALLOWED to
// run on exactly one CPU, then verify it stays there.
//
// Diff this against exp2_detect_cpu.cpp to see exactly what pinning added:
// the cpu_set_t block at the top of worker(), and nothing else.
//
// Usage: ./01_exp3_pinned [cpu]      (default CPU 4)

#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <sched.h>
#include <pthread.h>

constexpr long kIterations = 2'000'000'000;
constexpr long kCheckInterval = 1'000'000;
constexpr int  kDefaultPinCpu = 4;

void worker(int pin_to_cpu) {
    using clock = std::chrono::steady_clock;

    // A cpu_set_t is a bitmask of CPUs this thread is allowed to run on.
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);            // start with an empty set
    CPU_SET(pin_to_cpu, &cpuset); // allow only this one CPU

    // pthread_self() means "this thread" — we are inside worker(), so this
    // pins the worker, not main. Affinity restricts WHERE the thread may
    // run; it does not itself move the thread, though the next scheduling
    // decision will honor the restriction.
    int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
    if (rc != 0) {
        // pthread_* return an error number directly instead of setting errno.
        std::cerr << "pthread_setaffinity_np failed: " << std::strerror(rc) << "\n";
        return;
    }

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
                migrations++;
                last_cpu = current_cpu;
            }
        }
    }
    auto t1 = clock::now();

    double elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "requested:  CPU " << pin_to_cpu << "\n";
    std::cout << "start CPU:  " << start_cpu << "\n";
    std::cout << "final CPU:  " << last_cpu << "\n";
    std::cout << "migrations: " << migrations << "\n";
    std::cout << "iterations: " << kIterations << "\n";
    std::cout << "elapsed:    " << elapsed_ms << " ms\n";
    std::cout << "sink:       " << sink << "\n";
}

int main(int argc, char** argv) {
    int cpu = (argc > 1) ? std::atoi(argv[1]) : kDefaultPinCpu;

    // Catch a bad CPU here, with a clear message, instead of letting
    // pthread_setaffinity_np fail with an opaque EINVAL later.
    int ncpu = static_cast<int>(std::thread::hardware_concurrency());
    if (cpu < 0 || cpu >= ncpu) {
        std::cerr << "error: cpu " << cpu << " out of range (0.." << ncpu - 1 << ")\n";
        return 1;
    }

    std::cout << "=== Experiment 3: pinned to CPU " << cpu << " ===\n";
    std::thread t(worker, cpu);
    t.join();
    return 0;
}
