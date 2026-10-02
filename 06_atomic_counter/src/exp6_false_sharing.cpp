// Experiment 6 — False sharing with atomics: testing Project 05's prediction.
//
// Project 05 measured ~5x for two plain-store writers on private counters in
// one line, and explained the modest size with the store buffer: a plain
// store can retire into the store buffer while the line is away, and the
// core's own later load of the counter is forwarded from it, so the thread
// keeps running. It predicted that a locked RMW — which must own the line AT
// the operation and drains the store buffer — cannot hide that way, so its
// false-sharing penalty should be much larger, and a private-but-packed
// atomic counter should cost about what a truly SHARED one does.
//
// Five configurations, two threads on CPUs 2 and 4, same run, order rotating:
//
//   plain  packed / padded      — Project 05's measurement, re-taken here
//   fetch_add packed / padded   — the same layouts, locked RMW instead
//   fetch_add shared            — both threads on ONE counter (true sharing)
//
// Usage: ./06_exp6_false_sharing [cpuA cpuB]      (default CPUs 2 and 4)

#include <cstdlib>
#include <iomanip>
#include <iostream>

#include "atomic_counter.hpp"

constexpr std::uint64_t kIterations = 10'000'000;

int main(int argc, char** argv) {
    int cpu_a = argc > 2 ? std::atoi(argv[1]) : kDefaultCpuA;
    int cpu_b = argc > 2 ? std::atoi(argv[2]) : kDefaultCpuB;
    const std::vector<int> two = {cpu_a, cpu_b};

    PackedCounters<PlainCounter> plain_packed;
    PaddedCounters<PlainCounter> plain_padded;
    PackedCounters<FetchAddCounter> atomic_packed;
    PaddedCounters<FetchAddCounter> atomic_padded;
    FetchAddCounter atomic_shared;

    std::cout << "=== Experiment 6: false sharing, plain vs atomic ===\n";
    std::cout << "cpus " << cpu_a << " (core " << physical_core_of(cpu_a) << ") and " << cpu_b << " (core "
              << physical_core_of(cpu_b) << "), " << kIterations << " increments per thread, " << kTrials
              << " trials each, order rotating\n\n";

    std::vector<Contender> contenders = {
        private_contender(plain_packed, two, kIterations, "plain packed"),
        private_contender(plain_padded, two, kIterations, "plain padded"),
        private_contender(atomic_packed, two, kIterations, "fetch_add packed"),
        private_contender(atomic_padded, two, kIterations, "fetch_add padded"),
        shared_contender(atomic_shared, two, kIterations, "fetch_add shared"),
    };
    std::vector<Measurement> r = measure_interleaved(contenders);

    bool ok = true;
    std::cout << std::fixed << std::setprecision(3);
    std::cout << std::setw(18) << "config" << std::setw(10) << "min" << std::setw(10) << "p50" << std::setw(10)
              << "max" << "   (slowest-thread ns/increment)\n";
    for (std::size_t i = 0; i < contenders.size(); ++i) {
        ok &= r[i].ok();
        std::cout << std::setw(18) << contenders[i].name << std::setw(10) << r[i].slowest.min << std::setw(10)
                  << r[i].slowest.p50 << std::setw(10) << r[i].slowest.max << "\n";
    }
    std::cout << std::setprecision(2);
    std::cout << "plain     packed/padded p50: " << r[0].slowest.p50 / r[1].slowest.p50 << "x\n";
    std::cout << "fetch_add packed/padded p50: " << r[2].slowest.p50 / r[3].slowest.p50 << "x\n";
    std::cout << "fetch_add packed/shared p50: " << r[2].slowest.p50 / r[4].slowest.p50 << "x\n";
    std::cout << "checks: " << (ok ? "ok" : "FAILED (count or pinning)") << "\n";
    return ok ? 0 : 1;
}
