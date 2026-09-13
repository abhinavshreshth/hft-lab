// Experiment 4 — Compare Sequential vs Random, Same Run.
//
// Experiments 2 and 3 measure sequential and random order in separate
// process executions, so a difference between their numbers could in
// principle come from run-to-run conditions (CPU frequency state, other
// processes, thermal state) rather than from access order itself. This
// experiment removes that confound: both patterns are chased, for every
// size, inside one execution. The chase order (sequential-then-random or
// random-then-sequential) alternates by size so that "which pattern ran
// first and got any frequency-ramp warmup benefit" cannot systematically
// favor one pattern across the whole sweep. Both builders are held as
// ChainBuilder& in the ordering array — the loop that actually runs the
// chase never branches on which concrete pattern it is holding.

#include <array>
#include <iomanip>
#include <iostream>

#include "cache_lab.hpp"

int main() {
    std::cout << "=== Experiment 4: sequential vs random, same run ===\n\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << std::setw(10) << "size" << std::setw(14) << "elements" << std::setw(16)
              << "seq ns/access" << std::setw(16) << "rnd ns/access" << std::setw(12) << "rnd/seq"
              << "\n";

    SequentialChainBuilder sequential_builder;
    RandomChainBuilder random_builder(kRandomSeed);
    PointerChaser chaser;

    std::size_t sink = 0;
    const auto& sizes = sweep_sizes_bytes();
    for (std::size_t i = 0; i < sizes.size(); ++i) {
        std::size_t bytes = sizes[i];
        std::size_t n = node_count(bytes);
        auto warmup = warmup_steps_for(n);

        // Chase order alternates by size (see file header): each element
        // is a ChainBuilder&, so the loop below calls only the interface
        // and never needs to know which concrete pattern goes first.
        std::array<const ChainBuilder*, 2> order = (i % 2 == 0)
            ? std::array<const ChainBuilder*, 2>{&sequential_builder, &random_builder}
            : std::array<const ChainBuilder*, 2>{&random_builder, &sequential_builder};

        ChaseResult results[2];
        for (int slot = 0; slot < 2; ++slot) {
            auto nodes = order[slot]->build(n);
            results[slot] = chaser.chase(nodes, warmup, kMeasureSteps);
        }

        const ChaseResult& seq_result = (order[0] == &sequential_builder) ? results[0] : results[1];
        const ChaseResult& rnd_result = (order[0] == &random_builder) ? results[0] : results[1];
        double seq_ns = seq_result.ns_per_access;
        double rnd_ns = rnd_result.ns_per_access;

        // final_index (from ChaseResult) is folded into a running xor sink
        // and printed once at the end, so both chases' loads stay observably
        // used and neither can be dead-code-eliminated.
        sink ^= seq_result.final_index ^ rnd_result.final_index;

        std::cout << std::setw(8) << bytes / 1024 << " KiB" << std::setw(14) << n << std::setw(16)
                   << seq_ns << std::setw(16) << rnd_ns << std::setw(12) << (rnd_ns / seq_ns)
                   << "\n";
    }

    std::cout << "\nfinal indices xor-folded (prevents dead-code elimination): " << sink << "\n";

    return 0;
}
