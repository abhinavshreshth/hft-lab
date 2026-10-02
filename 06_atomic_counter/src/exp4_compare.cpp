// Experiment 4 — Every strategy, uncontended and contended, same run.
//
// Experiments 2 and 3 run in separate processes and cover only mutex and
// fetch_add. Here all four correct strategies (fetch_add, CAS loop,
// spinlock, mutex) race inside ONE execution, in two sections:
//
//   uncontended — 1 thread. What the synchronization itself costs when
//                 nobody else wants the counter. Project 05's plain
//                 volatile increment is included as the no-synchronization
//                 reference (it is only correct because there is one thread).
//   contended   — 2 threads on one shared counter, CPUs 2 and 4. What each
//                 strategy costs once the line has to move between cores.
//
// Within each section the contenders' order rotates every trial so none
// systematically gets the warmer machine.
//
// Usage: ./06_exp4_compare [cpuA cpuB]      (default CPUs 2 and 4)

#include <cstdlib>
#include <iomanip>
#include <iostream>

#include "atomic_counter.hpp"

constexpr std::uint64_t kUncontendedIterations = 20'000'000;
constexpr std::uint64_t kContendedIterations = 5'000'000;

int main(int argc, char** argv) {
    int cpu_a = argc > 2 ? std::atoi(argv[1]) : kDefaultCpuA;
    int cpu_b = argc > 2 ? std::atoi(argv[2]) : kDefaultCpuB;
    const std::vector<int> one = {cpu_a};
    const std::vector<int> two = {cpu_a, cpu_b};

    PlainCounter plain;
    FetchAddCounter fetch_add;
    CasCounter cas;
    SpinlockCounter spinlock;
    MutexCounter mutex;

    std::cout << "=== Experiment 4: all strategies, same run ===\n";
    std::cout << "uncontended: cpu " << cpu_a << ", " << kUncontendedIterations << " increments\n";
    std::cout << "contended:   cpus " << cpu_a << " (core " << physical_core_of(cpu_a) << ") and " << cpu_b
              << " (core " << physical_core_of(cpu_b) << ") on one counter, " << kContendedIterations
              << " increments per thread\n";
    std::cout << kTrials << " trials per strategy after " << kWarmupRaces
              << " discarded warmup race(s), order rotating\n\n";

    std::vector<Measurement> solo = measure_interleaved({
        shared_contender(plain, one, kUncontendedIterations),
        shared_contender(fetch_add, one, kUncontendedIterations),
        shared_contender(cas, one, kUncontendedIterations),
        shared_contender(spinlock, one, kUncontendedIterations),
        shared_contender(mutex, one, kUncontendedIterations),
    });
    std::vector<Measurement> duo = measure_interleaved({
        shared_contender(fetch_add, two, kContendedIterations),
        shared_contender(cas, two, kContendedIterations),
        shared_contender(spinlock, two, kContendedIterations),
        shared_contender(mutex, two, kContendedIterations),
    });

    const char* names[] = {PlainCounter::kName, FetchAddCounter::kName, CasCounter::kName, SpinlockCounter::kName,
                           MutexCounter::kName};
    bool ok = true;

    std::cout << std::fixed;
    std::cout << "-- uncontended (1 thread), ns/increment --\n";
    std::cout << std::setw(11) << "strategy" << std::setw(9) << "min" << std::setw(9) << "p50" << std::setw(9)
              << "max" << std::setw(10) << "vs plain\n";
    for (std::size_t i = 0; i < solo.size(); ++i) {
        const Measurement& m = solo[i];
        ok &= m.ok();
        std::cout << std::setprecision(3) << std::setw(11) << names[i] << std::setw(9) << m.slowest.min << std::setw(9)
                  << m.slowest.p50 << std::setw(9) << m.slowest.max << std::setprecision(1) << std::setw(8)
                  << m.slowest.p50 / solo[0].slowest.p50 << "x\n";
    }

    std::cout << "\n-- contended (2 threads, 1 shared counter) --\n";
    std::cout << std::setw(11) << "strategy" << std::setw(10) << "slow p50" << std::setw(10) << "slow min"
              << std::setw(10) << "slow max" << std::setw(10) << "fast p50" << std::setw(10) << "Mincr/s"
              << std::setw(9) << "vs solo" << std::setw(11) << "cas f/inc" << std::setw(10) << "vcsw/tr"
              << std::setw(10) << "icsw/tr" << std::setw(9) << "kernel\n";
    for (std::size_t i = 0; i < duo.size(); ++i) {
        const Measurement& m = duo[i];
        ok &= m.ok();
        std::cout << std::setprecision(3) << std::setw(11) << names[i + 1] << std::setw(10) << m.slowest.p50
                  << std::setw(10) << m.slowest.min << std::setw(10) << m.slowest.max << std::setw(10)
                  << m.fastest.p50 << std::setprecision(1) << std::setw(10) << m.aggregate_mops.p50 << std::setw(8)
                  << m.slowest.p50 / solo[i + 1].slowest.p50 << "x" << std::setprecision(3) << std::setw(11)
                  << m.cas_failures_per_increment << std::setprecision(1) << std::setw(10)
                  << m.voluntary_switches_per_trial << std::setw(10) << m.involuntary_switches_per_trial
                  << std::setw(8) << 100.0 * m.kernel_fraction << "%\n";
    }
    std::cout << "(slow/fast = slowest/fastest thread ns/increment; Mincr/s = total increments/s across both;\n"
              << " vs solo = contended slowest p50 / uncontended p50; cas f/inc = failed CAS attempts per increment;\n"
              << " vcsw/icsw per trial = voluntary (slept) / involuntary (preempted) context switches, both threads;\n"
              << " kernel = share of the threads' CPU time spent in the kernel during the loop)\n";
    std::cout << "\nchecks: " << (ok ? "ok" : "FAILED (count or pinning)") << "\n";
    return ok ? 0 : 1;
}
