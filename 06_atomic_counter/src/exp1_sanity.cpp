// Experiment 1 — Apparatus sanity check.
//
// Before trusting any timing from Experiments 2-6, this validates that the
// apparatus is what it claims to be, the same role Experiment 1 played in
// Projects 02-05:
//
// 1. std::atomic<uint64_t> is lock-free on this machine (no hidden lock
//    inside the "atomic"), and the counter types have the sizes the layouts
//    assume.
// 2. PackedCounters really puts all 8 counters in ONE cache line and
//    PaddedCounters really gives each its OWN line — for both counter types
//    Experiment 6 uses — checked from runtime addresses.
// 3. The topology the multi-thread experiments place threads on.
// 4. NEGATIVE CONTROL: two threads sharing a LoadStoreCounter (atomic load,
//    then a separate atomic store) must LOSE increments. If they did not, the
//    count check below would be incapable of failing and "every count exact"
//    would prove nothing.
// 5. Every real contender — fetch_add, CAS loop, spinlock, mutex — lands on
//    exactly threads x iterations with 2 and with 8 threads sharing one
//    counter, and every thread ran on its requested CPU.

#include <iomanip>
#include <iostream>
#include <set>

#include "atomic_counter.hpp"

constexpr std::uint64_t kIterations = 2'000'000;

template <class Layout>
bool all_same_line(Layout& layout) {
    for (std::size_t i = 1; i < kMaxThreads; ++i)
        if (line_of(layout.at(i)) != line_of(layout.at(0))) return false;
    return true;
}

template <class Layout>
bool all_distinct_lines(Layout& layout) {
    std::set<std::uintptr_t> lines;
    for (std::size_t i = 0; i < kMaxThreads; ++i) lines.insert(line_of(layout.at(i)));
    return lines.size() == kMaxThreads;
}

// Races `trials` times on one shared counter and reports whether every race
// was exact and pinned.
template <Counter C>
bool exact_when_shared(const std::vector<int>& cpus, int trials) {
    C counter;
    bool ok = true;
    for (int t = 0; t < trials; ++t) {
        RaceResult r = race(shared_by(counter, cpus.size()), cpus, kIterations);
        ok &= r.counts_ok() && r.pinned_ok;
    }
    std::cout << "  " << std::setw(10) << C::kName << "  " << cpus.size() << " threads x " << trials
              << " races: " << (ok ? "exact" : "WRONG COUNT OR PINNING") << "\n";
    return ok;
}

int main() {
    std::cout << "=== Experiment 1: apparatus sanity check ===\n\n";
    bool pass = true;

    std::cout << "-- atomics and sizes --\n";
    int sysfs_line = -1;
    std::ifstream("/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size") >> sysfs_line;
    std::cout << "  sysfs coherency_line_size:                " << sysfs_line << "  (kCacheLineSize " << kCacheLineSize
              << ")\n";
    std::cout << "  atomic<uint64_t>::is_always_lock_free:    " << std::boolalpha
              << std::atomic<std::uint64_t>::is_always_lock_free << "\n";
    std::cout << "  atomic<bool>::is_always_lock_free:        " << std::atomic<bool>::is_always_lock_free << "\n";
    std::cout << "  sizeof PlainCounter / FetchAddCounter / CasCounter: " << sizeof(PlainCounter) << " / "
              << sizeof(FetchAddCounter) << " / " << sizeof(CasCounter) << "\n";
    std::cout << "  sizeof SpinlockCounter / MutexCounter (std::mutex): " << sizeof(SpinlockCounter) << " / "
              << sizeof(MutexCounter) << " (" << sizeof(std::mutex) << ")\n";
    bool atomics_ok = sysfs_line == static_cast<int>(kCacheLineSize) &&
                      std::atomic<std::uint64_t>::is_always_lock_free && std::atomic<bool>::is_always_lock_free &&
                      sizeof(PlainCounter) == 8 && sizeof(FetchAddCounter) == 8;
    std::cout << (atomics_ok ? "PASS" : "FAIL") << ": 64-byte lines, lock-free atomics, 8-byte counters\n\n";
    pass &= atomics_ok;

    std::cout << "-- private-counter layouts (runtime addresses) --\n";
    PackedCounters<PlainCounter> packed_plain;
    PaddedCounters<PlainCounter> padded_plain;
    PackedCounters<FetchAddCounter> packed_atomic;
    PaddedCounters<FetchAddCounter> padded_atomic;
    bool layouts_ok = all_same_line(packed_plain) && all_same_line(packed_atomic) &&
                      all_distinct_lines(padded_plain) && all_distinct_lines(padded_atomic);
    std::cout << "  packed<fetch_add> counter offsets:";
    for (std::size_t i = 0; i < kMaxThreads; ++i)
        std::cout << " " << reinterpret_cast<std::uintptr_t>(packed_atomic.at(i)) -
                                reinterpret_cast<std::uintptr_t>(packed_atomic.at(0));
    std::cout << "  (base % 64 = " << reinterpret_cast<std::uintptr_t>(packed_atomic.at(0)) % kCacheLineSize << ")\n";
    std::cout << "  padded<fetch_add> counter offsets:";
    for (std::size_t i = 0; i < kMaxThreads; ++i)
        std::cout << " " << reinterpret_cast<std::uintptr_t>(padded_atomic.at(i)) -
                                reinterpret_cast<std::uintptr_t>(padded_atomic.at(0));
    std::cout << "  (base % 64 = " << reinterpret_cast<std::uintptr_t>(padded_atomic.at(0)) % kCacheLineSize << ")\n";
    std::cout << (layouts_ok ? "PASS" : "FAIL")
              << ": packed = 8 counters in one line, padded = one line each (plain and fetch_add)\n\n";
    pass &= layouts_ok;

    std::cout << "-- topology --\n";
    const int n_cpus = static_cast<int>(std::thread::hardware_concurrency());
    for (int cpu = 0; cpu < n_cpus; ++cpu) {
        std::cout << "  cpu " << std::setw(2) << cpu << " -> core " << physical_core_of(cpu)
                  << ((cpu % 4 == 3) ? "\n" : "   ");
    }
    std::vector<int> cores = one_cpu_per_core();
    std::cout << "  one_cpu_per_core():";
    for (int cpu : cores) std::cout << " " << cpu;
    std::cout << "\n";
    bool topology_ok = cores.size() >= kMaxThreads;
    std::cout << (topology_ok ? "PASS" : "FAIL") << ": at least " << kMaxThreads << " distinct physical cores\n\n";
    pass &= topology_ok;
    if (!topology_ok) cores.resize(kMaxThreads, kDefaultCpuA);
    std::vector<int> two = {kDefaultCpuA, kDefaultCpuB};
    std::vector<int> eight(cores.begin(), cores.begin() + static_cast<std::ptrdiff_t>(kMaxThreads));

    std::cout << "-- negative control: load+store on a shared counter must lose updates --\n";
    std::uint64_t min_lost = ~std::uint64_t{0}, max_lost = 0;
    bool control_pinned = true;
    LoadStoreCounter racy;
    for (int t = 0; t < kTrials; ++t) {
        RaceResult r = race(shared_by(racy, two.size()), two, kIterations);
        std::uint64_t lost = r.expected_total - r.observed_total;
        min_lost = std::min(min_lost, lost);
        max_lost = std::max(max_lost, lost);
        control_pinned &= r.pinned_ok;
    }
    std::cout << "  cpus " << two[0] << "," << two[1] << ": " << 2 * kIterations << " increments per race, "
              << kTrials << " races, lost " << min_lost << " .. " << max_lost << " ("
              << std::fixed << std::setprecision(1) << 100.0 * static_cast<double>(min_lost) / (2.0 * kIterations)
              << "% .. " << 100.0 * static_cast<double>(max_lost) / (2.0 * kIterations) << "%)\n";
    bool control_ok = min_lost > 0 && control_pinned;
    std::cout << (control_ok ? "PASS" : "FAIL") << ": every race lost updates — the count check can fail\n\n";
    pass &= control_ok;

    std::cout << "-- every contender is exact when shared --\n";
    bool exact = true;
    for (const auto& cpus : {two, eight}) {
        exact &= exact_when_shared<FetchAddCounter>(cpus, 3);
        exact &= exact_when_shared<CasCounter>(cpus, 3);
        exact &= exact_when_shared<SpinlockCounter>(cpus, 3);
        exact &= exact_when_shared<MutexCounter>(cpus, 3);
    }
    std::cout << (exact ? "PASS" : "FAIL") << ": fetch_add, cas_loop, spinlock and mutex never lost an increment\n\n";
    pass &= exact;

    std::cout << (pass ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED") << "\n";
    return pass ? 0 : 1;
}
