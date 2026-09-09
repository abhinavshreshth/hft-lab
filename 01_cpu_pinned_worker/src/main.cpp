#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <optional>
#include <string>
#include <cstdlib>
#include <sched.h>
#include <pthread.h>

// Which CPU "pinned" mode uses unless overridden on the command line.
constexpr int kDefaultPinCpu = 4;

// A thread runs inside the same process as main().
// It shares main's memory, but has its own stack and runs independently.
//
// std::nullopt means "don't pin" — this is the Experiment 1/2 baseline.
// A CPU number means "restrict this thread to exactly that CPU" — Experiment 3.
// optional<int> instead of a -1 sentinel: "no CPU" is then part of the type,
// not a magic number the reader has to know about.
void worker(std::optional<int> pin_to_cpu) {
    using clock = std::chrono::steady_clock;

    if (pin_to_cpu.has_value()) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);            // start with an empty allowed-CPU set
        CPU_SET(*pin_to_cpu, &cpuset); // allow only that one CPU in the set

        // pthread_self() here means "this thread" (the worker), because
        // this code is running inside worker() itself, not inside main().
        // Affinity restricts WHERE the thread is allowed to run — it does
        // not move the thread there immediately by itself, though in
        // practice the very next scheduling decision will honor it.
        int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
        if (rc != 0) {
            // pthread_* functions return an error number directly, unlike
            // most POSIX calls that set errno. strerror() decodes it.
            std::cerr << "pthread_setaffinity_np failed: " << std::strerror(rc) << "\n";
        }
    }

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

// The spec describes four experiments, but they are not four programs:
//   Experiment 1 (unpinned baseline)  -> mode "unpinned"
//   Experiment 2 (detect current CPU) -> the instrumentation inside worker(),
//                                        used by both modes, never removed
//   Experiment 3 (pin the worker)     -> mode "pinned"
//   Experiment 4 (compare)            -> mode "compare", plus the analysis
//                                        written up in README.md
// Each condition is runnable on its own so a results/ file can record the
// exact command that produced it.
void usage(const char* argv0) {
    std::cerr << "usage: " << argv0 << " [unpinned | pinned [cpu] | compare]\n"
              << "  unpinned      Experiment 1/2: no affinity, baseline\n"
              << "  pinned [cpu]  Experiment 3: pin to cpu (default "
              << kDefaultPinCpu << ")\n"
              << "  compare       Experiment 4: run both, back to back (default)\n";
}

// Runs the worker on its own thread and labels the output.
void run(const std::string& label, std::optional<int> pin_to_cpu) {
    std::cout << "=== " << label << " ===\n";
    std::thread t(worker, pin_to_cpu);
    t.join();
}

int main(int argc, char** argv) {
    std::string mode = (argc > 1) ? argv[1] : "compare";

    int cpu = kDefaultPinCpu;
    if (argc > 2) {
        cpu = std::atoi(argv[2]);
        // hardware_concurrency() is the logical CPU count. Catching a bad CPU
        // here gives a clear message instead of an opaque EINVAL from
        // pthread_setaffinity_np deep inside the worker.
        if (cpu < 0 || cpu >= static_cast<int>(std::thread::hardware_concurrency())) {
            std::cerr << "error: cpu " << cpu << " out of range (0.."
                      << std::thread::hardware_concurrency() - 1 << ")\n";
            return 1;
        }
    }

    if (mode == "unpinned") {
        run("unpinned (baseline)", std::nullopt);
    } else if (mode == "pinned") {
        run("pinned to CPU " + std::to_string(cpu), cpu);
    } else if (mode == "compare") {
        run("unpinned (baseline)", std::nullopt);
        std::cout << "\n";
        run("pinned to CPU " + std::to_string(cpu), cpu);
    } else {
        usage(argv[0]);
        return 1;
    }

    return 0;
}
