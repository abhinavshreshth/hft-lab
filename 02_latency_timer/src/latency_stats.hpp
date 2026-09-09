// Shared percentile calculator for Project 02's experiments.
//
// Unlike Project 01 (which duplicated its workload across every experiment
// file on purpose, so each file stayed an independently diffable artifact),
// this header IS shared: it is the fixed measurement apparatus this project
// exists to build, not part of the experimental progression being diffed.
// This is not a docs/CONVENTIONS.md section 6 promotion — that rule governs
// sharing code BETWEEN projects via common/include/hft/, which only happens
// once a second project needs it. Sharing a header within one project's own
// src/ is ordinary code organization. No hft:: namespace here either — that
// prefix is reserved for code that has actually been promoted.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

struct PercentileReport {
    std::size_t n;
    double min_ns;
    double p50_ns;
    double p95_ns;
    double p99_ns;
    double p999_ns;
    double max_ns;
};

// Nearest-rank percentile: rank = ceil(p/100 * n), 1-based, clamped to
// [1, n]. Chosen over interpolation because it always names a real observed
// sample rather than synthesizing a value between two neighbors.
inline double percentile(const std::vector<double>& sorted_ns, double p) {
    std::size_t n = sorted_ns.size();
    std::size_t rank = static_cast<std::size_t>(std::ceil(p / 100.0 * static_cast<double>(n)));
    if (rank < 1) rank = 1;
    if (rank > n) rank = n;
    return sorted_ns[rank - 1];
}

// Sorts samples_ns IN PLACE (ascending). Callers must only invoke this AFTER
// all timing is finished — sorting is O(n log n) and must never happen
// inside a timed region (docs/MEASUREMENT.md: don't measure the harness).
inline PercentileReport summarize(std::vector<double>& samples_ns) {
    std::sort(samples_ns.begin(), samples_ns.end());
    PercentileReport r;
    r.n       = samples_ns.size();
    r.min_ns  = samples_ns.front();
    r.p50_ns  = percentile(samples_ns, 50.0);
    r.p95_ns  = percentile(samples_ns, 95.0);
    r.p99_ns  = percentile(samples_ns, 99.0);
    r.p999_ns = percentile(samples_ns, 99.9);
    r.max_ns  = samples_ns.back();
    return r;
}
