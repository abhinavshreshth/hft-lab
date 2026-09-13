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
// favor one pattern across the whole sweep.

#include <iomanip>
#include <iostream>

#include "cache_lab.hpp"

int main() {
    std::cout << "=== Experiment 4: sequential vs random, same run ===\n\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << std::setw(10) << "size" << std::setw(14) << "elements" << std::setw(16)
              << "seq ns/access" << std::setw(16) << "rnd ns/access" << std::setw(12) << "rnd/seq"
              << "\n";

    std::size_t sink = 0;
    const auto& sizes = sweep_sizes_bytes();
    for (std::size_t i = 0; i < sizes.size(); ++i) {
        std::size_t bytes = sizes[i];
        std::size_t n = node_count(bytes);
        auto warmup = warmup_steps_for(n);

        // final_index (from ChaseResult) is folded into a running xor sink
        // and printed once at the end, so both chases' loads stay observably
        // used and neither can be dead-code-eliminated.
        double seq_ns, rnd_ns;
        std::size_t seq_final, rnd_final;
        if (i % 2 == 0) {
            auto seq_nodes = build_sequential_chain(n);
            auto seq_result = chase(seq_nodes, warmup, kMeasureSteps);
            seq_ns = seq_result.ns_per_access;
            seq_final = seq_result.final_index;
            auto rnd_nodes = build_random_chain(n, kRandomSeed);
            auto rnd_result = chase(rnd_nodes, warmup, kMeasureSteps);
            rnd_ns = rnd_result.ns_per_access;
            rnd_final = rnd_result.final_index;
        } else {
            auto rnd_nodes = build_random_chain(n, kRandomSeed);
            auto rnd_result = chase(rnd_nodes, warmup, kMeasureSteps);
            rnd_ns = rnd_result.ns_per_access;
            rnd_final = rnd_result.final_index;
            auto seq_nodes = build_sequential_chain(n);
            auto seq_result = chase(seq_nodes, warmup, kMeasureSteps);
            seq_ns = seq_result.ns_per_access;
            seq_final = seq_result.final_index;
        }
        sink ^= seq_final ^ rnd_final;

        std::cout << std::setw(8) << bytes / 1024 << " KiB" << std::setw(14) << n << std::setw(16)
                   << seq_ns << std::setw(16) << rnd_ns << std::setw(12) << (rnd_ns / seq_ns)
                   << "\n";
    }

    std::cout << "\nfinal indices xor-folded (prevents dead-code elimination): " << sink << "\n";

    return 0;
}
