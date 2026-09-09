// Experiment 2 — Baseline: naive aggregate-mean measurement.
//
// This is Project 01's measurement method (one t0/t1 around the whole loop)
// applied to a workload that is fast on almost every iteration but
// occasionally genuinely slow: std::vector::push_back on a vector that is
// NOT pre-reserved. Growth is amortized (most pushes are O(1)), but every
// ~log2(N)-th push triggers a real reallocation: malloc + memcpy of the
// whole buffer + free of the old one + first-touch of fresh pages.
//
// This experiment captures ZERO per-iteration data — it is structurally
// incapable of seeing whether that tail exists. That is the point: it is
// the baseline against which Experiment 3 (same workload, per-iteration
// timestamps) is compared.

#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

using clock_type = std::chrono::steady_clock;

constexpr long kIterations = 20'000;

void measure() {
    // Deliberately NOT reserved — growth must actually happen for the tail
    // this project is studying to exist.
    std::vector<std::uint64_t> v;

    auto t0 = clock_type::now();
    for (long i = 0; i < kIterations; ++i) {
        v.push_back(static_cast<std::uint64_t>(i));
    }
    auto t1 = clock_type::now();

    double elapsed_ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    double mean_ns = elapsed_ns / static_cast<double>(kIterations);

    std::cout << "iterations: " << kIterations << "\n";
    std::cout << "elapsed:    " << elapsed_ns / 1e6 << " ms\n";
    std::cout << "mean:       " << mean_ns << " ns/push_back\n";
    std::cout << "final size: " << v.size() << "\n";
}

int main() {
    std::cout << "=== Experiment 2: naive aggregate mean (baseline) ===\n";
    measure();
    return 0;
}
