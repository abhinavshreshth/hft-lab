// Shared CPU topology lookup, used by every experiment in this project to
// classify a migration as "same physical core" (SMT sibling) vs "cross
// core". Local to this project's src/ (like 02_latency_timer's
// latency_stats.hpp) rather than promoted to common/: it is the fixed
// apparatus this project builds, not part of the experimental progression.
#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace hft {

// Physical-core id for every logical CPU, read once from
// /sys/devices/system/cpu/cpuN/topology/core_id. Two logical CPUs with the
// same core_id are SMT siblings of the same physical core; the kernel
// exposes this directly, so there is no need to parse thread_siblings_list
// ranges by hand.
class CpuTopology {
public:
    CpuTopology() {
        int ncpu = static_cast<int>(std::thread::hardware_concurrency());
        core_id_.resize(ncpu, -1);
        for (int cpu = 0; cpu < ncpu; ++cpu) {
            std::string path = "/sys/devices/system/cpu/cpu" + std::to_string(cpu) +
                                "/topology/core_id";
            std::ifstream f(path);
            int id = -1;
            if (f) f >> id;
            core_id_[cpu] = id;
        }
    }

    int core_of(int cpu) const {
        return (cpu >= 0 && cpu < static_cast<int>(core_id_.size())) ? core_id_[cpu] : -1;
    }

    // True if cpu_a and cpu_b are SMT siblings of the same physical core.
    // A migration between them is the cheapest possible kind: same L1/L2,
    // same physical execution resources shared, only the SMT thread slot
    // changes.
    bool same_core(int cpu_a, int cpu_b) const { return core_of(cpu_a) == core_of(cpu_b); }

    int logical_cpu_count() const { return static_cast<int>(core_id_.size()); }

private:
    std::vector<int> core_id_;
};

}  // namespace hft
