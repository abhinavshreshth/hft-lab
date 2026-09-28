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

#include <array>
#include <cstdlib>
#include <iomanip>
#include <iostream>

#include "false_sharing.hpp"

constexpr std::uint64_t kIterations = 50'000'000;

int main(int argc, char** argv) {
    int cpu_a = argc > 2 ? std::atoi(argv[1]) : kDefaultCpuA;
    int cpu_b = argc > 2 ? std::atoi(argv[2]) : kDefaultCpuB;

    PackedLayout packed;
    PaddedLayout padded;
    RaceRunner runner;

    std::cout << "=== Experiment 4: packed vs padded, same run ===\n";
    std::cout << "cpus " << cpu_a << " (core " << physical_core_of(cpu_a) << ") and " << cpu_b << " (core "
              << physical_core_of(cpu_b) << "), " << kIterations << " increments per thread, " << kTrials
              << " trials each after " << kWarmupRaces
              << " discarded warmup race(s), order alternating\n\n";

    std::vector<double> packed_ns, padded_ns;
    bool ok = true;
    for (CounterLayout* layout : {static_cast<CounterLayout*>(&packed), static_cast<CounterLayout*>(&padded)}) {
        for (int w = 0; w < kWarmupRaces; ++w) {
            RaceResult r = runner.run(*layout, {cpu_a, cpu_b}, kIterations);
            ok &= r.counts_ok && r.pinned_ok;
        }
    }
    for (int t = 0; t < kTrials; ++t) {
        std::array<CounterLayout*, 2> order = (t % 2 == 0) ? std::array<CounterLayout*, 2>{&packed, &padded}
                                                           : std::array<CounterLayout*, 2>{&padded, &packed};
        for (CounterLayout* layout : order) {
            RaceResult r = runner.run(*layout, {cpu_a, cpu_b}, kIterations);
            ok &= r.counts_ok && r.pinned_ok;
            (layout == &packed ? packed_ns : padded_ns).push_back(r.slowest_ns_per_increment);
        }
    }

    TrialStats p = summarize_trials(packed_ns);
    TrialStats q = summarize_trials(padded_ns);

    std::cout << std::fixed << std::setprecision(3);
    std::cout << std::setw(8) << "layout" << std::setw(10) << "min" << std::setw(10) << "p50" << std::setw(10)
              << "max" << "   (slowest-thread ns/increment)\n";
    std::cout << std::setw(8) << "packed" << std::setw(10) << p.min << std::setw(10) << p.p50 << std::setw(10)
              << p.max << "\n";
    std::cout << std::setw(8) << "padded" << std::setw(10) << q.min << std::setw(10) << q.p50 << std::setw(10)
              << q.max << "\n";
    std::cout << "packed/padded p50: " << p.p50 / q.p50 << "x\n";
    std::cout << "checks: " << (ok ? "ok" : "FAILED (count or pinning)") << "\n";
    return ok ? 0 : 1;
}
