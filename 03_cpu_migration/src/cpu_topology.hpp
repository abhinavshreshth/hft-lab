// Shared CPU topology lookup, used by every experiment in this project to
// classify a migration as "same physical core" (SMT sibling) vs "cross
// core". Local to this project's src/ (like 02_latency_timer's
// latency_stats.hpp) rather than promoted to common/: it is the fixed
// apparatus this project builds, not part of the experimental progression.
#pragma once

#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace hft {

// Reads the physical-core id of a single logical CPU. CpuTopology depends
// on this abstraction rather than on sysfs directly (Dependency Inversion),
// so the read mechanism can be swapped -- e.g. a fake reader in a test --
// without touching the topology cache below (Open/Closed).
class CoreIdReader {
public:
    virtual ~CoreIdReader() = default;
    virtual int read_core_id(int logical_cpu) const = 0;
};

// Default strategy: reads /sys/devices/system/cpu/cpuN/topology/core_id.
// Two logical CPUs with the same core_id are SMT siblings of the same
// physical core; the kernel exposes this directly, so there is no need to
// parse thread_siblings_list ranges by hand.

//myNote: here read_core_id() function declation is important, 
//it is a pure virtual function, which means that any class that inherits
// from CoreIdReader must implement this function.
class SysfsCoreIdReader : public CoreIdReader {
public:
    int read_core_id(int logical_cpu) const override {
        const std::string core_id_path = "/sys/devices/system/cpu/cpu" +
                                          std::to_string(logical_cpu) + "/topology/core_id";
        std::ifstream core_id_file(core_id_path);
        int core_id = -1;
        if (core_id_file) core_id_file >> core_id;
        return core_id;
    }
};

// Caches each logical CPU's physical-core id for cheap repeated lookups.
// Sole responsibility is that cache and the same-core query built on it --
// the actual read strategy is injected (Single Responsibility).
class CpuTopology {
public:
    explicit CpuTopology(std::unique_ptr<CoreIdReader> core_id_reader =
                              std::make_unique<SysfsCoreIdReader>()) {
        const int logical_cpu_count = static_cast<int>(std::thread::hardware_concurrency());
        core_id_by_logical_cpu_.resize(logical_cpu_count, -1);
        for (int logical_cpu = 0; logical_cpu < logical_cpu_count; ++logical_cpu) {
            core_id_by_logical_cpu_[logical_cpu] = core_id_reader->read_core_id(logical_cpu);
        }
    }

    int core_of(int logical_cpu) const {
        const bool is_in_range =
            logical_cpu >= 0 && logical_cpu < static_cast<int>(core_id_by_logical_cpu_.size());
        return is_in_range ? core_id_by_logical_cpu_[logical_cpu] : -1;
    }

    // True if logical_cpu_a and logical_cpu_b are SMT siblings of the same
    // physical core. A migration between them is the cheapest possible
    // kind: same L1/L2, same physical execution resources shared, only the
    // SMT thread slot changes.
    bool same_core(int logical_cpu_a, int logical_cpu_b) const {
        return core_of(logical_cpu_a) == core_of(logical_cpu_b);
    }

    int logical_cpu_count() const { return static_cast<int>(core_id_by_logical_cpu_.size()); }

private:
    std::vector<int> core_id_by_logical_cpu_;
};

}  // namespace hft
