// Experiment 1 — Apparatus sanity check.
//
// Before trusting any timing from Experiments 2-7, this validates that the
// apparatus is what it claims to be, the same role Experiment 1 played in
// Projects 02-04:
//
// 1. The cache-line size this project hardcodes (kCacheLineSize) agrees with
//    what the compiler (std::hardware_destructive_interference_size, when the
//    standard library provides it) and the kernel (sysfs
//    coherency_line_size) say it is.
// 2. PackedLayout really puts all 8 counters in ONE cache line, PaddedLayout
//    really gives each counter its OWN line, and OffsetLayout(k) puts its two
//    counters in the same line exactly when k < 64 — checked from the actual
//    runtime addresses, not assumed from the struct definitions.
// 3. The topology the multi-thread experiments place threads on: which
//    logical CPUs are SMT siblings, and which CPUs one_cpu_per_core() picks.
// 4. With ONE thread there is nobody to share with, so packed and padded
//    must cost the same per increment. If they don't, the layout itself (not
//    sharing) is changing the cost, and every later comparison is suspect.
// 5. Every race leaves every counter at exactly `iterations` and every
//    thread on its requested CPU (Measurement::ok).

#include <fstream>
#include <iomanip>
#include <iostream>
#include <new>
#include <set>

#include "false_sharing.hpp"

constexpr std::uint64_t kIterations = 50'000'000;

bool all_same_line(CounterLayout& layout, std::size_t n) {
    for (std::size_t i = 1; i < n; ++i)
        if (line_of(layout.counter(i)) != line_of(layout.counter(0))) return false;
    return true;
}

bool all_distinct_lines(CounterLayout& layout, std::size_t n) {
    std::set<std::uintptr_t> lines;
    for (std::size_t i = 0; i < n; ++i) lines.insert(line_of(layout.counter(i)));
    return lines.size() == n;
}

void print_layout(CounterLayout& layout, std::size_t n) {
    auto base = reinterpret_cast<std::uintptr_t>(layout.counter(0));
    std::cout << "  " << layout.layout_name() << ":  base % 64 = " << base % kCacheLineSize << "\n";
    for (std::size_t i = 0; i < n; ++i) {
        auto addr = reinterpret_cast<std::uintptr_t>(layout.counter(i));
        std::cout << "    counter " << i << "  offset " << std::setw(4) << addr - base << " B  line +"
                  << line_of(layout.counter(i)) - line_of(layout.counter(0)) << "\n";
    }
}

int main() {
    std::cout << "=== Experiment 1: apparatus sanity check ===\n\n";
    bool pass = true;

    std::cout << "-- cache line size --\n";
    int sysfs_line = -1;
    std::ifstream("/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size") >> sysfs_line;
    std::cout << "  kCacheLineSize (this project):          " << kCacheLineSize << "\n";
    bool line_ok = sysfs_line == static_cast<int>(kCacheLineSize);
    // The standard library only declares this constant when the compiler
    // supplies a value for it (libstdc++ keys it off g++'s predefined
    // __GCC_DESTRUCTIVE_SIZE), so it is guarded by its feature-test macro.
    // g++ 15 defines it; IntelliSense's parser and older libc++ do not.
#ifdef __cpp_lib_hardware_interference_size
    std::cout << "  hardware_destructive_interference_size: " << std::hardware_destructive_interference_size << "\n";
    line_ok &= std::hardware_destructive_interference_size == kCacheLineSize;
    const char* agree_msg = ": all three agree\n\n";
#else
    std::cout << "  hardware_destructive_interference_size: not provided by this standard library\n";
    const char* agree_msg = ": kCacheLineSize and sysfs agree\n\n";
#endif
    std::cout << "  sysfs coherency_line_size:              " << sysfs_line << "\n";
    pass &= line_ok;
    std::cout << (line_ok ? "PASS" : "FAIL") << agree_msg;

    std::cout << "-- layouts (runtime addresses) --\n";
    PackedLayout packed;
    PaddedLayout padded;
    print_layout(packed, kMaxThreads);
    print_layout(padded, kMaxThreads);
    bool packed_ok = all_same_line(packed, kMaxThreads);
    bool padded_ok = all_distinct_lines(padded, kMaxThreads);
    std::cout << (packed_ok ? "PASS" : "FAIL") << ": packed puts all " << kMaxThreads << " counters in one line\n";
    std::cout << (padded_ok ? "PASS" : "FAIL") << ": padded puts each counter in its own line\n";
    bool offset_ok = true;
    for (std::size_t off = 8; off <= OffsetLayout::kMaxOffsetBytes; off += 8) {
        OffsetLayout layout(off);
        offset_ok &= (all_same_line(layout, 2) == (off < kCacheLineSize));
    }
    std::cout << (offset_ok ? "PASS" : "FAIL") << ": offset layout shares a line exactly when offset < 64 (8..512 B)\n\n";
    pass &= packed_ok && padded_ok && offset_ok;

    std::cout << "-- topology --\n";
    const int n_cpus = static_cast<int>(std::thread::hardware_concurrency());
    for (int cpu = 0; cpu < n_cpus; ++cpu) {
        std::cout << "  cpu " << std::setw(2) << cpu << " -> core " << physical_core_of(cpu)
                  << ((cpu % 4 == 3) ? "\n" : "   ");
    }
    std::cout << "  one_cpu_per_core():";
    for (int cpu : one_cpu_per_core()) std::cout << " " << cpu;
    std::cout << "\n\n";

    std::cout << "-- single thread: layout alone must not matter --\n";
    std::cout << std::fixed << std::setprecision(3);
    auto p = measure(packed, {kDefaultCpuA}, kIterations);
    auto q = measure(padded, {kDefaultCpuA}, kIterations);
    std::cout << "  cpu " << kDefaultCpuA << ", " << kIterations << " increments, " << kTrials << " trials\n";
    std::cout << "  packed ns/increment  min " << p.stats.min << "  p50 " << p.stats.p50 << "  max " << p.stats.max << "\n";
    std::cout << "  padded ns/increment  min " << q.stats.min << "  p50 " << q.stats.p50 << "  max " << q.stats.max << "\n";
    std::cout << "  packed/padded p50:   " << p.stats.p50 / q.stats.p50 << "\n";
    std::cout << ((p.ok && q.ok) ? "PASS" : "FAIL") << ": every counter reached " << kIterations
              << " and every thread ran on its pinned CPU\n\n";
    pass &= p.ok && q.ok;

    std::cout << (pass ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED") << "\n";
    return pass ? 0 : 1;
}
