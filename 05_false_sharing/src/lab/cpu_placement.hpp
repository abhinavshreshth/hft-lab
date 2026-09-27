// Which CPUs the racing threads run on: topology lookup, pinning, and the
// command-line override every two-thread experiment accepts.
#pragma once

#include <vector>

namespace false_sharing {

// Default placement for the two-thread experiments: two distinct physical
// cores (CPU 2 = core 1, CPU 4 = core 2 on this machine), away from CPU 0.
constexpr int kDefaultCpuA = 2;
constexpr int kDefaultCpuB = 4;

// Physical core id of a logical CPU, from sysfs. Two logical CPUs with the
// same id are SMT siblings (one L1d/L2). Returns -1 if unreadable.
int physical_core_of(int logical_cpu);

// The lowest-numbered logical CPU of each physical core, skipping core 0 (it
// handles more housekeeping interrupts): CPUs guaranteed to be on DISTINCT
// physical cores, so a thread sweep never puts two writers on one core.
std::vector<int> one_cpu_per_core();

int logical_cpu_count();

// Restricts the calling thread to exactly one CPU.
bool pin_current_thread(int cpu);

// argv[1..n] as CPU numbers if all n = defaults.size() were given, otherwise
// the defaults.
std::vector<int> cpus_from_args(int argc, char** argv, const std::vector<int>& defaults);

}  // namespace false_sharing
