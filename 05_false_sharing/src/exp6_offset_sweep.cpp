// Experiment 6 — How far apart is far enough?
//
// Experiments 2-5 compare two points: 8 bytes apart (packed) and a whole
// line apart (padded). This sweeps the distance between the two threads'
// counters from 8 bytes up to 512 bytes, same two CPUs, same loop, to find
// where the penalty actually stops. If the coherence unit is exactly the
// 64-byte line, every offset < 64 should cost the same as packed and every
// offset >= 64 the same as padded — a single cliff at 64. A second step at
// 128 would mean the hardware couples adjacent lines (e.g. an adjacent-line
// prefetcher pulling the neighbor line along), and that padding to 64 is
// not enough on this machine.
//
// Usage: ./05_exp6_offset_sweep [cpuA cpuB]      (default CPUs 2 and 4)

#include "lab/cpu_placement.hpp"
#include "lab/report.hpp"
#include "lab/trials.hpp"

using namespace false_sharing;

constexpr std::uint64_t kIterations = 20'000'000;

int main(int argc, char** argv) {
    std::vector<int> cpus = cpus_from_args(argc, argv, {kDefaultCpuA, kDefaultCpuB});

    const std::size_t offsets[] = {8, 16, 24, 32, 40, 48, 56, 64, 72, 96, 120, 128, 136, 192, 256, 512};

    std::vector<report::OffsetPoint> points;
    bool ok = true;
    for (std::size_t off : offsets) {
        OffsetLayout layout(off);
        bool same_line = line_of(layout.counter(0)) == line_of(layout.counter(1));
        Measurement m = measure(layout, cpus, kIterations);
        ok &= m.ok;
        points.push_back({off, same_line, m});
    }

    report::offset_sweep(cpus, kIterations, points, ok);
    return ok ? 0 : 1;
}
