// Experiment 7 — Does it matter WHICH other CPU the second thread is on?
//
// Experiments 2-6 place the two threads on two fixed physical cores. Here one
// thread stays on an anchor CPU and the partner is moved across every other
// logical CPU in turn, racing packed and padded at each placement. Three
// cases are expected to differ:
//
//   SMT sibling   — same physical core, same L1d. The line never has to
//                   leave the core, so there is no cross-core coherence
//                   traffic at all — but the two hyperthreads also compete
//                   for one core's execution resources, which is why padded
//                   is measured at every placement as the control.
//   other core    — the line must move between two cores' private caches
//                   through the shared L3.
//   other CCD     — the 7900X is physically two 6-core chiplets, each with
//                   its own L3; a line crossing between them goes over the
//                   Infinity Fabric. WSL2's sysfs reports one L3 shared by
//                   all 24 CPUs, so if that boundary exists it can only show
//                   up here, in the timings.
//
// Usage: ./05_exp7_placement_sweep [anchor_cpu]      (default CPU 2)

#include <array>
#include <cstdlib>
#include <iomanip>
#include <iostream>

#include "false_sharing.hpp"

constexpr std::uint64_t kIterations = 20'000'000;

int main(int argc, char** argv) {
    int anchor = argc > 1 ? std::atoi(argv[1]) : kDefaultCpuA;
    const int n_cpus = static_cast<int>(std::thread::hardware_concurrency());

    PackedLayout packed;
    PaddedLayout padded;

    std::cout << "=== Experiment 7: placement sweep ===\n";
    std::cout << "anchor cpu " << anchor << " (core " << physical_core_of(anchor) << "), " << kIterations
              << " increments per thread, " << kTrials << " trials per point\n\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << std::setw(8) << "partner" << std::setw(6) << "core" << std::setw(13) << "relation" << std::setw(12)
              << "padded p50" << std::setw(12) << "packed p50" << std::setw(12) << "packed max" << std::setw(10)
              << "ratio" << "   (slowest-thread ns/increment)\n";

    bool ok = true;
    int row = 0;
    for (int partner = 0; partner < n_cpus; ++partner) {
        if (partner == anchor) continue;
        std::array<CounterLayout*, 2> order = (row++ % 2 == 0) ? std::array<CounterLayout*, 2>{&packed, &padded}
                                                               : std::array<CounterLayout*, 2>{&padded, &packed};
        Measurement results[2];
        for (int slot = 0; slot < 2; ++slot) results[slot] = measure(*order[slot], {anchor, partner}, kIterations);
        const Measurement& p = (order[0] == &packed) ? results[0] : results[1];
        const Measurement& q = (order[0] == &padded) ? results[0] : results[1];
        ok &= p.ok && q.ok;

        bool sibling = physical_core_of(partner) == physical_core_of(anchor);
        std::cout << std::setw(8) << partner << std::setw(6) << physical_core_of(partner) << std::setw(13)
                  << (sibling ? "smt-sibling" : "other-core") << std::setw(12) << q.stats.p50 << std::setw(12)
                  << p.stats.p50 << std::setw(12) << p.stats.max << std::setw(9) << p.stats.p50 / q.stats.p50
                  << "x\n";
    }
    std::cout << "\nchecks: " << (ok ? "ok" : "FAILED (count or pinning)") << "\n";
    return ok ? 0 : 1;
}
