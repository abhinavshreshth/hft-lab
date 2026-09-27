#include "race_runner.hpp"

#include <sched.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

#include "cpu_placement.hpp"

namespace false_sharing {

namespace {

void race_worker(volatile std::uint64_t* c, int cpu, std::uint64_t iterations, std::atomic<std::size_t>& ready,
                 std::atomic<bool>& go, char& pinned, double& ns_per_increment) {
    pinned = pin_current_thread(cpu) && sched_getcpu() == cpu;
    ready.fetch_add(1, std::memory_order_acq_rel);
    while (!go.load(std::memory_order_acquire)) {
    }

    auto t0 = std::chrono::steady_clock::now();
    for (std::uint64_t k = 0; k < iterations; ++k) {
        *c = *c + 1;
    }
    auto t1 = std::chrono::steady_clock::now();

    ns_per_increment = std::chrono::duration<double, std::nano>(t1 - t0).count() / static_cast<double>(iterations);
}

}  // namespace

RaceResult RaceRunner::run(CounterLayout& layout, const std::vector<int>& cpus, std::uint64_t iterations) const {
    const std::size_t n = cpus.size();
    RaceResult result;
    result.ns_per_increment.assign(n, 0.0);
    if (n == 0 || n > layout.max_threads()) {
        result.counts_ok = false;
        return result;
    }

    for (std::size_t i = 0; i < n; ++i) *layout.counter(i) = 0;

    std::atomic<std::size_t> ready{0};
    std::atomic<bool> go{false};
    std::vector<char> pinned(n, 0);
    std::vector<std::thread> threads;
    threads.reserve(n);

    for (std::size_t i = 0; i < n; ++i) {
        threads.emplace_back(race_worker, layout.counter(i), cpus[i], iterations, std::ref(ready), std::ref(go),
                             std::ref(pinned[i]), std::ref(result.ns_per_increment[i]));
    }

    // Start barrier: wait until every worker is pinned and spinning, then
    // release them together so the timed loops genuinely overlap.
    while (ready.load(std::memory_order_acquire) < n) std::this_thread::yield();
    go.store(true, std::memory_order_release);
    for (auto& t : threads) t.join();

    for (std::size_t i = 0; i < n; ++i) {
        result.counts_ok &= (*layout.counter(i) == iterations);
        result.pinned_ok &= (pinned[i] != 0);
        result.slowest_ns_per_increment = std::max(result.slowest_ns_per_increment, result.ns_per_increment[i]);
    }
    return result;
}

}  // namespace false_sharing
