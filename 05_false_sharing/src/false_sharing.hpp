// Shared false-sharing apparatus for Project 05's experiments. Like
// 04_cache_latency's cache_lab.hpp, this is the fixed measurement instrument
// this project builds (N pinned threads, each hammering its OWN counter),
// shared via plain #include across every binary that needs it. No hft::
// namespace — that prefix is reserved for code actually promoted to
// common/include/hft/ (docs/CONVENTIONS.md §6), which this is not.
//
// The split mirrors cache_lab.hpp's ChainBuilder / PointerChaser:
//
//   CounterLayout — WHERE each thread's counter lives in memory (the
//                   experimental variable: packed into one cache line,
//                   padded one-per-line, or at an arbitrary byte offset).
//   RaceRunner    — HOW the threads are pinned, started together and timed
//                   (fixed apparatus, identical for every layout).
//
// RaceRunner depends only on the CounterLayout interface (Dependency
// Inversion), so an experiment picks its layout by constructing one concrete
// class at exactly one call site — see the one-line diff between
// exp2_packed.cpp and exp3_padded.cpp.
//
// No thread ever touches another thread's counter. There is no data race and
// no logical sharing — any slowdown is purely the hardware's coherence
// protocol moving a cache LINE between cores because two unrelated counters
// happen to sit in it. That is what "false" sharing means.
#pragma once

#include <pthread.h>
#include <sched.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

// docs/ENVIRONMENT.md / sysfs coherency_line_size: 64 bytes on this machine.
// Hardcoded rather than taken from std::hardware_destructive_interference_size
// so the value under test is visible in the source; exp1 prints both and
// checks they agree.
constexpr std::size_t kCacheLineSize = 64;

// 8 x uint64_t fill one 64-byte line exactly, so up to 8 threads can all be
// packed into a single line — the upper bound of every thread-count sweep.
constexpr std::size_t kMaxThreads = kCacheLineSize / sizeof(std::uint64_t);

// ---------------------------------------------------------------------------
// Layouts
// ---------------------------------------------------------------------------

// Where thread i's counter lives. volatile: every increment must be a real
// load + store to memory (not hoisted into a register by -O2), because the
// store is what pulls the cache line into this core in Modified state.
// Volatile — not std::atomic — on purpose: each counter has exactly one
// writer, so there is no race to make atomic, and atomics are Project 06.
class CounterLayout {
public:
    virtual ~CounterLayout() = default;
    virtual volatile std::uint64_t* counter(std::size_t thread_index) = 0;
    virtual std::size_t max_threads() const = 0;
    virtual const char* layout_name() const = 0;
};

// Baseline: all counters adjacent in one 64-byte-aligned block — the layout
// `std::uint64_t counters[N];` gives you by default. Every counter shares one
// cache line with every other.
struct alignas(kCacheLineSize) PackedBlock {
    std::uint64_t value[kMaxThreads];
};
static_assert(sizeof(PackedBlock) == kCacheLineSize, "packed block must be exactly one line");

class PackedLayout : public CounterLayout {
public:
    volatile std::uint64_t* counter(std::size_t i) override { return &block_.value[i]; }
    std::size_t max_threads() const override { return kMaxThreads; }
    const char* layout_name() const override { return "packed"; }

private:
    PackedBlock block_{};
};

// Modified: the standard fix. alignas(64) on the element type pads sizeof up
// to 64 and aligns each element to a line boundary, so each counter owns a
// whole cache line and no two threads' counters can ever share one.
struct alignas(kCacheLineSize) PaddedCounter {
    std::uint64_t value;
};
static_assert(sizeof(PaddedCounter) == kCacheLineSize, "padded counter must own one line");

class PaddedLayout : public CounterLayout {
public:
    volatile std::uint64_t* counter(std::size_t i) override { return &counters_[i].value; }
    std::size_t max_threads() const override { return kMaxThreads; }
    const char* layout_name() const override { return "padded"; }

private:
    PaddedCounter counters_[kMaxThreads]{};
};

// Two counters, the second offset_bytes after the first, inside a
// line-aligned buffer. Used by the offset sweep to find exactly where the
// penalty stops: offsets below 64 put both counters in the same line,
// offsets of 64+ put them in different lines. offset_bytes must be a
// non-zero multiple of 8 (two uint64_t cannot overlap) and <= kMaxOffsetBytes.
class OffsetLayout : public CounterLayout {
public:
    static constexpr std::size_t kMaxOffsetBytes = 512;

    explicit OffsetLayout(std::size_t offset_bytes) : offset_words_(offset_bytes / sizeof(std::uint64_t)) {}

    volatile std::uint64_t* counter(std::size_t i) override { return &words_[i * offset_words_]; }
    std::size_t max_threads() const override { return 2; }
    const char* layout_name() const override { return "offset"; }

private:
    std::size_t offset_words_;
    alignas(kCacheLineSize) std::uint64_t words_[(kMaxOffsetBytes + kCacheLineSize) / sizeof(std::uint64_t)]{};
};

// Which cache line (address / 64) a counter falls in. Used by exp1 to verify
// the layouts are what they claim before any timing is trusted.
inline std::uintptr_t line_of(const volatile std::uint64_t* p) {
    return reinterpret_cast<std::uintptr_t>(p) / kCacheLineSize;
}

// ---------------------------------------------------------------------------
// Topology / pinning
// ---------------------------------------------------------------------------

// Physical core id of a logical CPU, from sysfs. Two logical CPUs with the
// same core id are SMT siblings: they share one L1d/L2, so a line "moving"
// between them never leaves the core. Returns -1 if unreadable.
inline int physical_core_of(int logical_cpu) {
    std::ifstream f("/sys/devices/system/cpu/cpu" + std::to_string(logical_cpu) + "/topology/core_id");
    int core = -1;
    if (f) f >> core;
    return core;
}

// The lowest-numbered logical CPU of each physical core, skipping core 0
// (CPU 0 handles more housekeeping interrupts). Gives a list of CPUs that are
// guaranteed to be on DISTINCT physical cores — no SMT siblings — so a
// thread-count sweep never accidentally puts two writers on one core.
inline std::vector<int> one_cpu_per_core() {
    const int n = static_cast<int>(std::thread::hardware_concurrency());
    std::vector<int> seen_cores;
    std::vector<int> cpus;
    for (int cpu = 0; cpu < n; ++cpu) {
        int core = physical_core_of(cpu);
        if (core <= 0) continue;
        if (std::find(seen_cores.begin(), seen_cores.end(), core) != seen_cores.end()) continue;
        seen_cores.push_back(core);
        cpus.push_back(cpu);
    }
    return cpus;
}

inline bool pin_current_thread(int cpu) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    return pthread_setaffinity_np(pthread_self(), sizeof(set), &set) == 0;
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

struct RaceResult {
    std::vector<double> ns_per_increment;  // one per thread
    double slowest_ns_per_increment = 0.0;  // the headline: the race ends when the slowest thread does
    bool counts_ok = true;                  // every counter == iterations afterwards
    bool pinned_ok = true;                  // every thread landed on its requested CPU
};

// Sole responsibility: start one thread per entry in cpus, pinned there,
// release them all at once, and time each thread's increment loop. Knows
// nothing about which layout it was handed (Dependency Inversion).
//
// Timing is per thread, inside the thread, around the increment loop only —
// thread creation, pinning and the start barrier are outside the timed
// region (docs/MEASUREMENT.md: don't measure the harness). Per-increment
// timing is deliberately NOT done: an uncontended increment is ~1 ns, below
// steady_clock's 10 ns resolution (Project 02), so the loop is timed as a
// whole and divided.
class RaceRunner {
public:
    RaceResult run(CounterLayout& layout, const std::vector<int>& cpus, std::uint64_t iterations) const {
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
            threads.emplace_back(worker, layout.counter(i), cpus[i], iterations, std::ref(ready),
                                 std::ref(go), std::ref(pinned[i]), std::ref(result.ns_per_increment[i]));
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

private:
    static void worker(volatile std::uint64_t* c, int cpu, std::uint64_t iterations, std::atomic<std::size_t>& ready,
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
};

// ---------------------------------------------------------------------------
// Trials
// ---------------------------------------------------------------------------

// Each configuration is raced kTrials times inside one process; the report is
// the spread of the slowest-thread ns/increment across those trials. With
// ~10 trials p95/p99/p99.9 would all just be the max, so only min/p50/max
// are reported (docs/MEASUREMENT.md's full percentile set applies to
// per-event latencies, which this deliberately does not sample).
constexpr int kTrials = 11;

struct TrialStats {
    double min = 0.0;
    double p50 = 0.0;
    double max = 0.0;
};

inline TrialStats summarize_trials(std::vector<double> samples) {
    std::sort(samples.begin(), samples.end());
    return {samples.front(), samples[samples.size() / 2], samples.back()};
}

// Warmup: one full race per configuration, run and discarded before the
// timed trials — page-faults the counters in, brings the line into the
// caches involved and lets the cores' clocks ramp before anything counts.
constexpr int kWarmupRaces = 1;

// Runs kWarmupRaces discarded races, then `trials` timed races of one
// configuration, and summarizes the timed ones. ok is false if any race
// (warmup included) failed its count or pinning check.
struct Measurement {
    TrialStats stats;
    bool ok = true;
};

inline Measurement measure(CounterLayout& layout, const std::vector<int>& cpus, std::uint64_t iterations,
                           int trials = kTrials) {
    RaceRunner runner;
    std::vector<double> samples;
    Measurement m;
    for (int w = 0; w < kWarmupRaces; ++w) {
        RaceResult r = runner.run(layout, cpus, iterations);
        m.ok &= r.counts_ok && r.pinned_ok;
    }
    for (int t = 0; t < trials; ++t) {
        RaceResult r = runner.run(layout, cpus, iterations);
        m.ok &= r.counts_ok && r.pinned_ok;
        samples.push_back(r.slowest_ns_per_increment);
    }
    m.stats = summarize_trials(samples);
    return m;
}

// Default placement for the two-thread experiments: two distinct physical
// cores (CPU 2 = core 1, CPU 4 = core 2 on this machine — see exp1's
// topology print), away from CPU 0. Overridable on the command line.
constexpr int kDefaultCpuA = 2;
constexpr int kDefaultCpuB = 4;
