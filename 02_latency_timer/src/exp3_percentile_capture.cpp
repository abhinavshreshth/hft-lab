// Experiment 3 — Modified: per-iteration latency capture + percentiles.
//
// Diff this against exp2_naive_mean.cpp: identical workload (unreserved
// std::vector<uint64_t>, kIterations push_backs), exactly one change —
// every push_back is individually timestamped, and the full distribution
// is reported via min/p50/p95/p99/p99.9/max instead of a single mean.
//
// Two design points that must not be glossed over:
//
// 1. samples_ns.reserve(kIterations) is load-bearing. If the MEASUREMENT
//    vector were allowed to grow organically, its own reallocations would
//    inject spikes into the very data being collected — the harness would
//    contaminate the phenomenon under study (docs/MEASUREMENT.md: don't
//    measure the harness).
//
// 2. Warmup is deliberately 0 iterations here, unlike exp1's 100-sample
//    warmup. Vector-doubling reallocations are front-loaded (they occur at
//    push counts 1, 2, 4, 8, 16, ...), so discarding "the first N
//    iterations" would delete most of the very events under study.
//    docs/MEASUREMENT.md's warmup rule carves out exactly this case:
//    "...unless per-iteration latency is itself the subject" — which it is.

#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

#include "latency_stats.hpp"

using clock_type = std::chrono::steady_clock;

constexpr long kIterations = 20'000;

void measure() {
    std::vector<std::uint64_t> v;             // deliberately NOT reserved — same as exp2
    std::vector<double> samples_ns;
    samples_ns.reserve(kIterations);           // load-bearing — see file header comment

    std::size_t prev_capacity = v.capacity();
    long reallocations = 0;

    for (long i = 0; i < kIterations; ++i) {
        auto t0 = clock_type::now();
        v.push_back(static_cast<std::uint64_t>(i));
        auto t1 = clock_type::now();

        samples_ns.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count());

        if (v.capacity() != prev_capacity) {
            reallocations++;
            prev_capacity = v.capacity();
        }
    }

    auto report = summarize(samples_ns);

    std::cout << "iterations:          " << kIterations << "\n";
    std::cout << "reallocations seen:  " << reallocations << "\n";
    std::cout << "min   (ns):          " << report.min_ns << "\n";
    std::cout << "p50   (ns):          " << report.p50_ns << "\n";
    std::cout << "p95   (ns):          " << report.p95_ns << "\n";
    std::cout << "p99   (ns):          " << report.p99_ns << "\n";
    std::cout << "p99.9 (ns):          " << report.p999_ns << "\n";
    std::cout << "max   (ns):          " << report.max_ns << "\n";
    std::cout << "final size:          " << v.size() << "\n";
}

int main() {
    std::cout << "=== Experiment 3: per-iteration percentile capture (modified) ===\n";
    measure();
    return 0;
}
