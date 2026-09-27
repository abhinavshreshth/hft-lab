// HOW the threads race — the fixed apparatus, identical for every layout.
#pragma once

#include <cstdint>
#include <vector>

#include "counter_layout.hpp"

namespace false_sharing {

struct RaceResult {
    std::vector<double> ns_per_increment;  // one per thread
    double slowest_ns_per_increment = 0.0;  // the headline: the race ends when the slowest thread does
    bool counts_ok = true;                  // every counter == iterations afterwards
    bool pinned_ok = true;                  // every thread landed on its requested CPU
};

// Starts one thread per entry in cpus, pinned there, releases them all at
// once, and times each thread's increment loop. Knows nothing about which
// layout it was handed (Dependency Inversion).
//
// Timing is per thread, inside the thread, around the increment loop only —
// thread creation, pinning and the start barrier are outside the timed
// region (docs/MEASUREMENT.md: don't measure the harness). Per-increment
// timing is deliberately NOT done: an uncontended increment is ~0.2 ns, far
// below steady_clock's 10 ns resolution (Project 02), so the loop is timed
// as a whole and divided.
class RaceRunner {
public:
    RaceResult run(CounterLayout& layout, const std::vector<int>& cpus, std::uint64_t iterations) const;
};

}  // namespace false_sharing
