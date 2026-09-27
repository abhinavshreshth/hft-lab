#include "cpu_placement.hpp"

#include <pthread.h>
#include <sched.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <string>
#include <thread>

namespace false_sharing {

int physical_core_of(int logical_cpu) {
    std::ifstream f("/sys/devices/system/cpu/cpu" + std::to_string(logical_cpu) + "/topology/core_id");
    int core = -1;
    if (f) f >> core;
    return core;
}

int logical_cpu_count() { return static_cast<int>(std::thread::hardware_concurrency()); }

std::vector<int> one_cpu_per_core() {
    std::vector<int> seen_cores;
    std::vector<int> cpus;
    for (int cpu = 0; cpu < logical_cpu_count(); ++cpu) {
        int core = physical_core_of(cpu);
        if (core <= 0) continue;
        if (std::find(seen_cores.begin(), seen_cores.end(), core) != seen_cores.end()) continue;
        seen_cores.push_back(core);
        cpus.push_back(cpu);
    }
    return cpus;
}

bool pin_current_thread(int cpu) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    return pthread_setaffinity_np(pthread_self(), sizeof(set), &set) == 0;
}

std::vector<int> cpus_from_args(int argc, char** argv, const std::vector<int>& defaults) {
    if (argc - 1 < static_cast<int>(defaults.size())) return defaults;
    std::vector<int> cpus;
    for (std::size_t i = 0; i < defaults.size(); ++i) cpus.push_back(std::atoi(argv[i + 1]));
    return cpus;
}

}  // namespace false_sharing
