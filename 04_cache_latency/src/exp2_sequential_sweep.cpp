// Experiment 2 — Baseline: sequential-order pointer chase across the size
// sweep. next[i] = (i + 1) % n visits cache lines in increasing address
// order — the pattern a hardware prefetcher is built to detect and run
// ahead of, regardless of which cache tier the working set actually fits
// in. This is the "everything looks fast" baseline that Experiment 3's
// random order is diffed against.
//
// This file and exp3_random_sweep.cpp differ by exactly one line: which
// concrete ChainBuilder is constructed. The sweep loop itself only ever
// calls the ChainBuilder/PointerChaser interfaces (cache_lab.hpp's
// Dependency Inversion) — it would work unchanged with any future access
// pattern that implements ChainBuilder.

#include <iomanip>
#include <iostream>
#include <memory>

#include "cache_lab.hpp"

int main() {
    std::unique_ptr<ChainBuilder> builder = std::make_unique<SequentialChainBuilder>();
    PointerChaser chaser;

    std::cout << "=== Experiment 2: " << builder->pattern_name() << " sweep (baseline) ===\n\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << std::setw(10) << "size" << std::setw(14) << "elements" << std::setw(16)
              << "ns/access" << std::setw(12) << "final" << "\n";

    for (std::size_t bytes : sweep_sizes_bytes()) {
        std::size_t n = node_count(bytes);
        auto nodes = builder->build(n);
        auto result = chaser.chase(nodes, warmup_steps_for(n), kMeasureSteps);

        // final_index is printed (not discarded) so the loads in chase()'s
        // measured loop are observably used and cannot be dead-code-eliminated
        // (docs/MEASUREMENT.md: "do not let the compiler delete your benchmark").
        std::cout << std::setw(8) << bytes / 1024 << " KiB" << std::setw(14) << n << std::setw(16)
                   << result.ns_per_access << std::setw(12) << result.final_index << "\n";
    }

    return 0;
}
