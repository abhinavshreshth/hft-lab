// Experiment 4 — Compare Baseline vs Pinned  (docx section 13)
//
// Runs the SAME workload twice, back to back, changing exactly one thing:
// whether the worker is pinned. Everything else is held constant, so any
// difference can only come from affinity.
//
// The analysis of these numbers lives in README.md sections 7-8, because
// interpreting a result is writing, not code.
//
// Usage: ./01_exp4_compare [cpu]     (default CPU 4)

#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <optional>
#include <string>
#include <sched.h>
#include <pthread.h>

constexpr long kIterations = 2'000'000'000;
constexpr long kCheckInterval = 1'000'000;
constexpr int  kDefaultPinCpu = 4;

// std::nullopt = do not pin (the Experiment 1/2 condition).
// A CPU number  = pin to that CPU (the Experiment 3 condition).
// optional rather than a -1 sentinel: "no CPU" lives in the type, so there
// is no magic number for the reader to decode.
void worker(std::optional<int> pin_to_cpu) {
    using clock = std::chrono::steady_clock;

    if (pin_to_cpu.has_value()) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(*pin_to_cpu, &cpuset);
        int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
        if (rc != 0) {
            std::cerr << "pthread_setaffinity_np failed: " << std::strerror(rc) << "\n";
            return;
        }
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

    std::cout << "start CPU:  " << start_cpu << "\n";
    std::cout << "final CPU:  " << last_cpu << "\n";
    std::cout << "migrations: " << migrations << "\n";
    std::cout << "iterations: " << kIterations << "\n";
    std::cout << "elapsed:    " << elapsed_ms << " ms\n";
    std::cout << "sink:       " << sink << "\n";
}

void run(const std::string& label, std::optional<int> pin_to_cpu) {
    std::cout << "--- " << label << " ---\n";
    std::thread t(worker, pin_to_cpu);
    t.join();
}

int main(int argc, char** argv) {
    int cpu = (argc > 1) ? std::atoi(argv[1]) : kDefaultPinCpu;

    int ncpu = static_cast<int>(std::thread::hardware_concurrency());
    if (cpu < 0 || cpu >= ncpu) {
        std::cerr << "error: cpu " << cpu << " out of range (0.." << ncpu - 1 << ")\n";
        return 1;
    }

    std::cout << "=== Experiment 4: compare baseline vs pinned ===\n";
    run("unpinned (baseline)", std::nullopt);
    std::cout << "\n";
    run("pinned to CPU " + std::to_string(cpu), cpu);

    return 0;
}
