// Experiment 5 — Thread-count scaling: one shared counter vs a sharded one.
//
// Experiments 2-4 use at most two threads. Here 1 to 8 threads, each on a
// DISTINCT physical core (one_cpu_per_core(), no SMT siblings), hammer one
// counter with each of the four correct strategies. The question is what a
// single shared counter does to total throughput as cores are added, and
// whether the strategies degrade differently: fetch_add is one RMW per
// increment whatever happens; a CAS loop's failed attempts should multiply
// with more competitors; a mutex puts losers to sleep.
//
// The fifth contender is the fix: "sharded" — each thread fetch_adds its OWN
// line-padded slot (PaddedCounters<FetchAddCounter>), and a reader would sum
// the slots. It is still an atomic RMW per increment, but no line is shared,
// so it isolates "atomic" from "shared".
//
// All five race at every thread count in the same run, order rotating. At
// the full thread count each strategy's per-thread p50 is printed too, by
// CPU: a shared line is a serial resource, and this shows whether the cores
// get equal turns at it.

#include <iomanip>
#include <iostream>

#include "atomic_counter.hpp"

constexpr std::uint64_t kIterations = 2'000'000;

int main() {
    std::vector<int> cpus = one_cpu_per_core();
    if (cpus.size() < kMaxThreads) {
        std::cerr << "need " << kMaxThreads << " distinct physical cores, found " << cpus.size() << "\n";
        return 1;
    }
    cpus.resize(kMaxThreads);

    FetchAddCounter fetch_add;
    CasCounter cas;
    SpinlockCounter spinlock;
    MutexCounter mutex;
    PaddedCounters<FetchAddCounter> sharded;

    std::cout << "=== Experiment 5: thread-count scaling ===\n";
    std::cout << "cpus (one per physical core, in order used):";
    for (int c : cpus) std::cout << " " << c;
    std::cout << "\n" << kIterations << " increments per thread, " << kTrials
              << " trials per point, order rotating\n\n";

    std::cout << std::fixed;
    std::cout << std::setw(8) << "threads" << std::setw(11) << "strategy" << std::setw(10) << "slow p50"
              << std::setw(10) << "fast p50" << std::setw(10) << "Mincr/s" << std::setw(10) << "agg min"
              << std::setw(10) << "agg max" << std::setw(11) << "cas f/inc" << std::setw(10) << "vcsw/tr"
              << std::setw(9) << "kernel\n";

    bool ok = true;
    std::vector<Contender> last_contenders;
    std::vector<Measurement> last_results;
    for (std::size_t n = 1; n <= kMaxThreads; ++n) {
        std::vector<int> used(cpus.begin(), cpus.begin() + static_cast<std::ptrdiff_t>(n));
        std::vector<Contender> contenders = {
            shared_contender(fetch_add, used, kIterations),
            shared_contender(cas, used, kIterations),
            shared_contender(spinlock, used, kIterations),
            shared_contender(mutex, used, kIterations),
            private_contender(sharded, used, kIterations, "sharded"),
        };
        std::vector<Measurement> results = measure_interleaved(contenders);
        for (std::size_t i = 0; i < contenders.size(); ++i) {
            const Measurement& m = results[i];
            ok &= m.ok();
            std::cout << std::setw(8) << n << std::setw(11) << contenders[i].name << std::setprecision(3)
                      << std::setw(10) << m.slowest.p50 << std::setw(10) << m.fastest.p50 << std::setprecision(1)
                      << std::setw(10) << m.aggregate_mops.p50 << std::setw(10) << m.aggregate_mops.min
                      << std::setw(10) << m.aggregate_mops.max << std::setprecision(3) << std::setw(11)
                      << m.cas_failures_per_increment << std::setprecision(1) << std::setw(10)
                      << m.voluntary_switches_per_trial << std::setw(8) << 100.0 * m.kernel_fraction << "%\n";
        }
        last_contenders = contenders;
        last_results = results;
    }
    std::cout << "(slow/fast p50 = slowest/fastest thread ns/increment; Mincr/s = total increments/s, p50 with\n"
              << " min/max across trials; cas f/inc = failed CAS attempts per increment; vcsw/tr = voluntary\n"
              << " context switches per trial, all threads; kernel = share of thread CPU time in the kernel)\n";

    std::cout << "\n-- per-thread p50 ns/increment at " << kMaxThreads << " threads --\n";
    std::cout << std::setw(11) << "cpu (core)";
    for (int c : cpus) std::cout << std::setw(5) << c << " (" << std::setw(2) << physical_core_of(c) << ")";
    std::cout << "\n";
    for (std::size_t i = 0; i < last_contenders.size(); ++i) {
        std::cout << std::setw(11) << last_contenders[i].name << std::setprecision(1);
        for (double ns : last_results[i].per_thread_p50) std::cout << std::setw(10) << ns;
        std::cout << "\n";
    }
    std::cout << "\nchecks: " << (ok ? "ok" : "FAILED (count or pinning)") << "\n";
    return ok ? 0 : 1;
}
