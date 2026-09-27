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

#include "lab/cpu_placement.hpp"
#include "lab/report.hpp"
#include "lab/trials.hpp"

using namespace false_sharing;

constexpr std::uint64_t kIterations = 20'000'000;

int main(int argc, char** argv) {
    int anchor = cpus_from_args(argc, argv, {kDefaultCpuA})[0];

    PackedLayout packed;
    PaddedLayout padded;

    std::vector<report::PlacementPoint> points;
    bool ok = true;
    for (int partner = 0; partner < logical_cpu_count(); ++partner) {
        if (partner == anchor) continue;
        bool packed_first = points.size() % 2 == 0;
        Comparison c = measure_both(packed, padded, {anchor, partner}, kIterations, packed_first);
        ok &= c.ok();
        bool smt_sibling = physical_core_of(partner) == physical_core_of(anchor);
        points.push_back({partner, smt_sibling, c});
    }

    report::placement_sweep(anchor, kIterations, points, ok);
    return ok ? 0 : 1;
}
