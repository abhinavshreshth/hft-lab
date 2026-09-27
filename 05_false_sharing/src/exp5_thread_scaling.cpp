// Experiment 5 — Thread-count scaling.
//
// Experiments 2-4 use exactly two writers. Here the number of writers varies
// from 1 to 8 (8 uint64_t fill one line, so packed stays a single line at
// every count), each on a DISTINCT physical core (one_cpu_per_core(), no SMT
// siblings). Padded is the control: each thread owns its line, so per-thread
// cost should stay flat as threads are added. Packed puts every writer on
// the same line, so the question is how the per-increment cost grows with
// the number of cores fighting over it.
//
// Both layouts are raced at every thread count in the same run, order
// alternating by thread count, as in Experiment 4.

#include <array>
#include <iomanip>
#include <iostream>

#include "false_sharing.hpp"

constexpr std::uint64_t kIterations = 20'000'000;

int main() {
    std::vector<int> cpus = one_cpu_per_core();
    if (cpus.size() < kMaxThreads) {
        std::cerr << "need " << kMaxThreads << " distinct physical cores, found " << cpus.size() << "\n";
        return 1;
    }
    cpus.resize(kMaxThreads);

    PackedLayout packed;
    PaddedLayout padded;

    std::cout << "=== Experiment 5: thread-count scaling ===\n";
    std::cout << "cpus (one per physical core, in order used):";
    for (int c : cpus) std::cout << " " << c;
    std::cout << "\n" << kIterations << " increments per thread, " << kTrials << " trials per point\n\n";

    std::cout << std::fixed << std::setprecision(3);
    std::cout << std::setw(8) << "threads" << std::setw(12) << "padded p50" << std::setw(12) << "packed p50"
              << std::setw(12) << "packed min" << std::setw(12) << "packed max" << std::setw(10) << "ratio"
              << "   (slowest-thread ns/increment)\n";

    bool ok = true;
    for (std::size_t n = 1; n <= kMaxThreads; ++n) {
        std::vector<int> used(cpus.begin(), cpus.begin() + static_cast<std::ptrdiff_t>(n));
        std::array<CounterLayout*, 2> order = (n % 2 == 1) ? std::array<CounterLayout*, 2>{&packed, &padded}
                                                           : std::array<CounterLayout*, 2>{&padded, &packed};
        Measurement results[2];
        for (int slot = 0; slot < 2; ++slot) results[slot] = measure(*order[slot], used, kIterations);
        const Measurement& p = (order[0] == &packed) ? results[0] : results[1];
        const Measurement& q = (order[0] == &padded) ? results[0] : results[1];
        ok &= p.ok && q.ok;

        std::cout << std::setw(8) << n << std::setw(12) << q.stats.p50 << std::setw(12) << p.stats.p50
                  << std::setw(12) << p.stats.min << std::setw(12) << p.stats.max << std::setw(9)
                  << p.stats.p50 / q.stats.p50 << "x\n";
    }
    std::cout << "\nchecks: " << (ok ? "ok" : "FAILED (count or pinning)") << "\n";
    return ok ? 0 : 1;
}
