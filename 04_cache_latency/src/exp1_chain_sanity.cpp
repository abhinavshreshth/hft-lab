// Experiment 1 — Apparatus sanity check.
//
// Before trusting any latency number from Experiments 2-4, this validates
// the measurement apparatus itself, the same role Experiment 1 played in
// Project 02 (clock resolution) and Project 03 (topology ground truth):
//
// 1. build_random_chain() must produce a single N-cycle for every size in
//    the sweep — if Sattolo's algorithm were replaced by plain Fisher-Yates,
//    it could silently produce several short disjoint cycles instead, and
//    every "random" latency number downstream would actually be measuring
//    a small, easily-cached loop.
// 2. At the smallest size (well inside L1), sequential and random order
//    should cost about the same per access — both fit entirely in L1 after
//    warmup, so access *order* should not matter yet. If it does, the
//    harness itself (not the cache hierarchy) is introducing a difference,
//    and Experiments 2-4's results would not be trustworthy.

#include <iomanip>
#include <iostream>

#include "cache_lab.hpp"

int main() {
    std::cout << "=== Experiment 1: chain-construction sanity check ===\n\n";

    std::cout << "-- single-cycle validation (build_random_chain) --\n";
    bool all_single_cycle = true;
    for (std::size_t bytes : sweep_sizes_bytes()) {
        std::size_t n = node_count(bytes);
        auto nodes = build_random_chain(n, kRandomSeed);
        bool ok = is_single_cycle(nodes);
        all_single_cycle &= ok;
        std::cout << "  " << std::setw(10) << bytes / 1024 << " KiB  n=" << std::setw(9) << n
                   << "  single-cycle: " << (ok ? "yes" : "NO") << "\n";
    }
    std::cout << (all_single_cycle ? "PASS" : "FAIL")
               << ": every random chain is a single N-cycle\n\n";

    std::cout << "-- order-independence at L1 residency --\n";
    std::size_t smallest = sweep_sizes_bytes().front();
    std::size_t n = node_count(smallest);

    auto seq_nodes = build_sequential_chain(n);
    auto rnd_nodes = build_random_chain(n, kRandomSeed);

    auto seq = chase(seq_nodes, warmup_steps_for(n), kMeasureSteps);
    auto rnd = chase(rnd_nodes, warmup_steps_for(n), kMeasureSteps);

    std::cout << "  size:                " << smallest / 1024 << " KiB (n=" << n << ")\n";
    std::cout << "  sequential ns/access: " << seq.ns_per_access << "\n";
    std::cout << "  random     ns/access: " << rnd.ns_per_access << "\n";
    std::cout << "  final indices consumed (prevents dead-code elimination): "
               << seq.final_index << ", " << rnd.final_index << "\n";

    return 0;
}
