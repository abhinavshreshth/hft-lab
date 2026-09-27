// Experiment 4 — Compare Packed vs Padded, Same Run.
//
// Experiments 2 and 3 run in separate processes, so a difference between
// them could in principle come from run-to-run machine state (frequency,
// background load) rather than layout. Here both layouts are raced on the
// same two CPUs inside ONE execution, trial by trial, and the order
// alternates each trial (packed-first, padded-first, ...) so neither layout
// systematically gets the warmer machine. Both are held as CounterLayout*:
// the loop that races them never branches on which layout it has.
//
// Usage: ./05_exp4_compare [cpuA cpuB]      (default CPUs 2 and 4)

#include "lab/cpu_placement.hpp"
#include "lab/report.hpp"
#include "lab/trials.hpp"

using namespace false_sharing;

constexpr std::uint64_t kIterations = 50'000'000;

int main(int argc, char** argv) {
    std::vector<int> cpus = cpus_from_args(argc, argv, {kDefaultCpuA, kDefaultCpuB});

    PackedLayout packed;
    PaddedLayout padded;

    Comparison c = measure_interleaved(packed, padded, cpus, kIterations);

    report::same_run_comparison(cpus, kIterations, c);
    return c.ok() ? 0 : 1;
}
