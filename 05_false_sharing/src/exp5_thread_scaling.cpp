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

#include "lab/cpu_placement.hpp"
#include "lab/report.hpp"
#include "lab/trials.hpp"

using namespace false_sharing;

constexpr std::uint64_t kIterations = 20'000'000;

int main() {
    std::vector<int> cpus = one_cpu_per_core();
    if (cpus.size() < kMaxThreads) {
        report::not_enough_cores(kMaxThreads, cpus.size());
        return 1;
    }
    cpus.resize(kMaxThreads);

    PackedLayout packed;
    PaddedLayout padded;

    std::vector<report::ScalingPoint> points;
    bool ok = true;
    for (std::size_t n = 1; n <= kMaxThreads; ++n) {
        std::vector<int> used(cpus.begin(), cpus.begin() + static_cast<std::ptrdiff_t>(n));
        Comparison c = measure_both(packed, padded, used, kIterations, /*a_first=*/n % 2 == 1);
        ok &= c.ok();
        points.push_back({n, c});
    }

    report::thread_scaling(cpus, kIterations, points, ok);
    return ok ? 0 : 1;
}
