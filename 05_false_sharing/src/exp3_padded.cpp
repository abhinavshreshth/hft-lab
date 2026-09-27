// Experiment 3 — Modified: two threads, each counter padded to its own cache line.
//
// Identical to Experiment 2 — same two pinned threads, same private
// counters, same increment loop — except each counter is an alignas(64)
// PaddedCounter, so the two counters sit in different cache lines and
// neither core's stores ever invalidate the other core's line.
//
// Diff this against exp2_packed.cpp: the only change is which CounterLayout
// is constructed.
//
// Usage: ./05_exp3_padded [cpuA cpuB]      (default CPUs 2 and 4)

#include "lab/cpu_placement.hpp"
#include "lab/report.hpp"
#include "lab/trials.hpp"

using namespace false_sharing;

constexpr std::uint64_t kIterations = 50'000'000;

int main(int argc, char** argv) {
    std::vector<int> cpus = cpus_from_args(argc, argv, {kDefaultCpuA, kDefaultCpuB});

    PaddedLayout layout;

    Measurement m = measure(layout, cpus, kIterations);

    report::two_thread_run("Experiment 3", layout, cpus, kIterations, m);
    return m.ok ? 0 : 1;
}
