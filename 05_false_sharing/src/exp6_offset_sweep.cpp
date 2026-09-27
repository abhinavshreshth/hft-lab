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

#include <cstdlib>
#include <iomanip>
#include <iostream>

#include "false_sharing.hpp"

constexpr std::uint64_t kIterations = 20'000'000;

int main(int argc, char** argv) {
    int cpu_a = argc > 2 ? std::atoi(argv[1]) : kDefaultCpuA;
    int cpu_b = argc > 2 ? std::atoi(argv[2]) : kDefaultCpuB;

    const std::size_t offsets[] = {8, 16, 24, 32, 40, 48, 56, 64, 72, 96, 120, 128, 136, 192, 256, 512};

    std::cout << "=== Experiment 6: offset sweep ===\n";
    std::cout << "cpus " << cpu_a << " and " << cpu_b << ", " << kIterations << " increments per thread, "
              << kTrials << " trials per offset\n\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << std::setw(8) << "offset" << std::setw(11) << "same line" << std::setw(10) << "min" << std::setw(10)
              << "p50" << std::setw(10) << "max" << "   (slowest-thread ns/increment)\n";

    bool ok = true;
    for (std::size_t off : offsets) {
        OffsetLayout layout(off);
        bool same = line_of(layout.counter(0)) == line_of(layout.counter(1));
        auto m = measure(layout, {cpu_a, cpu_b}, kIterations);
        ok &= m.ok;
        std::cout << std::setw(6) << off << " B" << std::setw(11) << (same ? "yes" : "no") << std::setw(10)
                  << m.stats.min << std::setw(10) << m.stats.p50 << std::setw(10) << m.stats.max << "\n";
    }
    std::cout << "\nchecks: " << (ok ? "ok" : "FAILED (count or pinning)") << "\n";
    return ok ? 0 : 1;
}
