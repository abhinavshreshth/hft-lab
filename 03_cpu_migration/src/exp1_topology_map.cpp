// Experiment 1 — Topology Map
//
// Not a comparison: this establishes the ground truth the rest of this
// project's migration detectors classify against. Prints, for every logical
// CPU, which physical core it belongs to (via CpuTopology, see
// cpu_topology.hpp), grouped so SMT sibling pairs are visible directly.
//
// Analogous to 02_latency_timer's exp1 (measuring the clock's real
// resolution before trusting anything built on top of it): here, before
// classifying a migration as "same core" or "cross core", we first have to
// know, and print, what the machine's actual core layout is — never assume
// logical CPU N and N+1 are siblings.

#include <iostream>
#include <map>
#include <vector>

#include "cpu_topology.hpp"

int main() {
    std::cout << "=== Experiment 1: CPU topology map ===\n";

    hft::CpuTopology topo;
    int ncpu = topo.logical_cpu_count();

    std::map<int, std::vector<int>> core_to_cpus;
    for (int cpu = 0; cpu < ncpu; ++cpu) {
        core_to_cpus[topo.core_of(cpu)].push_back(cpu);
    }

    std::cout << "logical CPUs:  " << ncpu << "\n";
    std::cout << "physical cores: " << core_to_cpus.size() << "\n\n";

    for (const auto& [core_id, cpus] : core_to_cpus) {
        std::cout << "core " << core_id << ": [";
        for (size_t i = 0; i < cpus.size(); ++i) {
            std::cout << cpus[i];
            if (i + 1 < cpus.size()) std::cout << ", ";
        }
        std::cout << "]\n";
    }

    return 0;
}
