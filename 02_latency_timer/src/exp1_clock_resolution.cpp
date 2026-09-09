// Experiment 1 — Measure steady_clock's real tick granularity.
//
// docs/ENVIRONMENT.md carries a placeholder: "steady_clock resolution:
// (measure in Project 02)". This experiment produces that number. A
// std::chrono::steady_clock has nanosecond REPRESENTATION, but the
// underlying clocksource ticks in coarser, hardware/kernel-defined steps.
// The only way to know the real granularity is to busy-poll now() and see
// how small an observed jump actually gets.
//
// Method: call now() in a tight loop; whenever the value differs from the
// last-seen value, record the size of that jump. The minimum observed jump
// (after discarding early warmup samples) is the practical resolution.

#include <chrono>
#include <iostream>
#include <vector>

#include "latency_stats.hpp"

using clock_type = std::chrono::steady_clock;

// Discard the first few tick-changes: the very first now() calls pay a
// vDSO/page-touch cost that isn't part of the clock's steady-state
// granularity.
constexpr int kWarmupSamples = 100;
constexpr int kSamples = 10'000;

void measure() {
    std::vector<double> deltas_ns;
    deltas_ns.reserve(kSamples);

    auto last = clock_type::now();
    int warmup_left = kWarmupSamples;
    int collected = 0;

    while (collected < kSamples) {
        auto now = clock_type::now();
        if (now != last) {
            double delta_ns = std::chrono::duration<double, std::nano>(now - last).count();
            if (warmup_left > 0) {
                warmup_left--;
            } else {
                deltas_ns.push_back(delta_ns);
                collected++;
            }
            last = now;
        }
    }

    auto report = summarize(deltas_ns);

    std::cout << "samples:    " << report.n << "\n";
    std::cout << "min (ns):   " << report.min_ns << "   <- practical steady_clock resolution\n";
    std::cout << "p50  (ns):  " << report.p50_ns << "\n";
    std::cout << "p95  (ns):  " << report.p95_ns << "\n";
    std::cout << "p99  (ns):  " << report.p99_ns << "\n";
    std::cout << "p99.9(ns):  " << report.p999_ns << "\n";
    std::cout << "max  (ns):  " << report.max_ns << "\n";
}

int main() {
    std::cout << "=== Experiment 1: steady_clock tick resolution ===\n";
    measure();
    return 0;
}
