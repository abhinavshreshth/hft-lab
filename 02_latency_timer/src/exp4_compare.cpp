// Experiment 4 — One run, reported two ways.
//
// Rather than running the baseline (exp2) and modified (exp3) conditions as
// two separate invocations — which would leave run-to-run noise as a
// possible confound — this runs the IDENTICAL single pass of per-iteration-
// timestamped push_backs once, and reports it two ways from the same
// dataset:
//
//   Framing A: naive wall-clock mean (elapsed / N)   — what Project 01's
//              method, and exp2, would report.
//   Framing B: the full percentile report from the per-iteration samples
//              collected during that same loop            — what exp3 adds.
//
// Because both numbers are derived from one execution, any gap between
// "what the mean says" and "what p99/p99.9/max say" is real, not
// measurement variance between separate runs.
//
// This differs from Project 01's exp4, which ran the same workload twice
// under two different runtime CONDITIONS (unpinned vs pinned). Here there
// is only one condition; the two "framings" are two different READINGS of
// one dataset, not two configurations.

#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

#include "latency_stats.hpp"

using clock_type = std::chrono::steady_clock;

constexpr long kIterations = 20'000;

void measure() {
    std::vector<std::uint64_t> v;             // deliberately NOT reserved
    std::vector<double> samples_ns;
    samples_ns.reserve(kIterations);           // load-bearing — see exp3

    auto t_wall0 = clock_type::now();
    for (long i = 0; i < kIterations; ++i) {
        auto t0 = clock_type::now();
        v.push_back(static_cast<std::uint64_t>(i));
        auto t1 = clock_type::now();
        samples_ns.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count());
    }
    auto t_wall1 = clock_type::now();

    double wall_elapsed_ns = std::chrono::duration<double, std::nano>(t_wall1 - t_wall0).count();
    double naive_mean_ns = wall_elapsed_ns / static_cast<double>(kIterations);

    auto report = summarize(samples_ns);

    std::cout << "iterations:               " << kIterations << "\n";
    std::cout << "-- Framing A: naive mean (Project 01 style) --\n";
    std::cout << "naive mean (ns):          " << naive_mean_ns << "\n";
    std::cout << "-- Framing B: percentile report, same run --\n";
    std::cout << "min   (ns):               " << report.min_ns << "\n";
    std::cout << "p50   (ns):               " << report.p50_ns << "\n";
    std::cout << "p95   (ns):               " << report.p95_ns << "\n";
    std::cout << "p99   (ns):               " << report.p99_ns << "\n";
    std::cout << "p99.9 (ns):               " << report.p999_ns << "\n";
    std::cout << "max   (ns):               " << report.max_ns << "\n";
    std::cout << "final size:               " << v.size() << "\n";
}

int main() {
    std::cout << "=== Experiment 4: one run, two framings ===\n";
    measure();
    return 0;
}
