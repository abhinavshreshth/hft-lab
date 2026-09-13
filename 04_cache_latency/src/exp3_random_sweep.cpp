// Experiment 3 — Modified: random-order pointer chase across the same size
// sweep as Experiment 2. Diff against exp2_sequential_sweep.cpp: the only
// change is which ChainBuilder is constructed — SequentialChainBuilder ->
// RandomChainBuilder — i.e. which order the same N cache lines are visited
// in. The sweep loop below is otherwise identical, because both builders
// satisfy the same ChainBuilder interface (cache_lab.hpp's Open/Closed and
// Liskov Substitution: PointerChaser and this loop don't need to know, or
// change, which pattern they were handed).
//
// Hypothesis (written *before* measuring): a hardware prefetcher can only
// exploit a predictable address stream. RandomChainBuilder's single
// Sattolo cycle makes the next cache line unknowable until the current
// node's value is actually loaded, so once the working set exceeds
// whatever tier can hold it, every step should show that tier's real
// miss latency instead of the near-flat numbers Experiment 2 is expected
// to show. Concretely: ns/access should stay low and roughly flat through
// the L1 sizes (4-32 KiB), start climbing once the set exceeds L1, climb
// again past the L2 boundary (1 MiB), and climb sharply past the L3
// boundary (32 MiB) into what should look like flat, high, RAM-latency
// numbers for 64-128 MiB.

#include <iomanip>
#include <iostream>
#include <memory>

#include "cache_lab.hpp"

int main() {
    std::unique_ptr<ChainBuilder> builder = std::make_unique<RandomChainBuilder>(kRandomSeed);
    PointerChaser chaser;

    std::cout << "=== Experiment 3: " << builder->pattern_name() << " sweep (modified) ===\n\n";
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
