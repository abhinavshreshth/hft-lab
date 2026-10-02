// Shared atomic-counter apparatus for Project 06's experiments. Like
// 05_false_sharing's false_sharing.hpp, this is the fixed measurement
// instrument this project builds (N pinned threads racing on a counter),
// shared via plain #include across every binary that needs it. No hft::
// namespace — that prefix is reserved for code actually promoted to
// common/include/hft/ (docs/CONVENTIONS.md §6), which this is not.
//
// The split mirrors false_sharing.hpp's CounterLayout / RaceRunner:
//
//   Counter  — HOW one increment is made safe (the experimental variable:
//              an atomic fetch_add, a CAS loop, a spinlock, a std::mutex,
//              or deliberately unsafe load-then-store).
//   race()   — HOW the threads are pinned, started together and timed
//              (fixed apparatus, identical for every counter).
//
// One difference from Project 05: Counter is a C++20 concept, not a virtual
// base class. In 05 the variable was WHERE the counter lived, so the hot
// loop could run on a raw pointer and the interface was only consulted at
// setup. Here the variable IS the operation inside the hot loop, and a
// virtual call per increment would add a few ns to every strategy — the same
// order as the thing being measured. race<C>() is instantiated once per
// counter type, so each strategy's increment() is inlined into its own loop.
// The dependency direction is unchanged: race() is written against the
// concept and never names a concrete counter.
//
// Memory orders: counters use relaxed (a pure count needs no ordering), locks
// use acquire/release (the minimum that makes them locks). Why those are the
// right orders is Project 07's subject. On x86 every locked RMW is a full
// barrier whatever order is requested, so the choice does not change the
// instructions this project measures.
#pragma once

#include <immintrin.h>  // _mm_pause
#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// docs/ENVIRONMENT.md / sysfs coherency_line_size: 64 bytes on this machine.
// Validated against sysfs in exp1, as in Project 05.
constexpr std::size_t kCacheLineSize = 64;

// Upper bound of every thread-count sweep. 8 x 8-byte counters fill one line
// exactly, so Experiment 6's packed layout stays a single line, and it
// matches Project 05's sweep so the two projects' tables line up.
constexpr std::size_t kMaxThreads = 8;

// ---------------------------------------------------------------------------
// Counters
// ---------------------------------------------------------------------------

// What race() needs from a counter. increment() returns the number of FAILED
// attempts it made before succeeding — non-zero only for CasCounter, and a
// compile-time 0 (optimized away) for everything else — so the CAS retry
// rate can be reported without the runner knowing which counter it has.
template <class C>
concept Counter = requires(C& c) {
    { C::kName } -> std::convertible_to<const char*>;
    { c.increment() } -> std::same_as<std::uint64_t>;
    { c.value() } -> std::same_as<std::uint64_t>;
    c.reset();
};

// Reference only, never shared: Project 05's plain volatile increment, a
// real load + add + store with no lock prefix. Safe only while each counter
// has exactly one writer (Experiment 4's single-thread row, Experiment 6's
// private counters). Two threads on one PlainCounter would be a data race.
class PlainCounter {
public:
    static constexpr const char* kName = "plain";
    std::uint64_t increment() {
        v_ = v_ + 1;
        return 0;
    }
    std::uint64_t value() { return v_; }
    void reset() { v_ = 0; }

private:
    volatile std::uint64_t v_ = 0;
};

// Negative control: an atomic LOAD followed by a separate atomic STORE. Each
// access is individually atomic — no data race, no undefined behavior — but
// the increment as a whole is not: another thread's increment can land
// between this thread's load and store and be overwritten. This is what
// `x = x + 1` on a shared variable does at the hardware level, made legal.
// Experiment 1 uses it to prove the count check can actually catch lost
// updates. Never timed as a contender — its result is wrong.
class LoadStoreCounter {
public:
    static constexpr const char* kName = "load+store";
    std::uint64_t increment() {
        v_.store(v_.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
        return 0;
    }
    std::uint64_t value() { return v_.load(); }
    void reset() { v_.store(0); }

private:
    std::atomic<std::uint64_t> v_{0};
};

// The atomic read-modify-write: one `lock add` instruction. The lock prefix
// makes the core hold the line exclusively for the whole load-add-store, so
// no other core's write can interleave — and it drains the store buffer
// first, so unlike PlainCounter it cannot hide a missing line behind it.
class FetchAddCounter {
public:
    static constexpr const char* kName = "fetch_add";
    std::uint64_t increment() {
        v_.fetch_add(1, std::memory_order_relaxed);
        return 0;
    }
    std::uint64_t value() { return v_.load(); }
    void reset() { v_.store(0); }

private:
    std::atomic<std::uint64_t> v_{0};
};

// The general-purpose atomic pattern: read, compute, and publish only if
// nobody changed the value in between (`lock cmpxchg`), otherwise retry with
// the value that won. For a counter fetch_add does the same job in one
// instruction; CAS is here because it is what every lock-free structure more
// complex than a counter is built from, and its retries are measurable.
// compare_exchange_weak: on x86 it never fails spuriously, and a loop is
// needed anyway.
class CasCounter {
public:
    static constexpr const char* kName = "cas_loop";
    std::uint64_t increment() {
        std::uint64_t failures = 0;
        std::uint64_t expected = v_.load(std::memory_order_relaxed);
        while (!v_.compare_exchange_weak(expected, expected + 1, std::memory_order_relaxed)) ++failures;
        return failures;
    }
    std::uint64_t value() { return v_.load(); }
    void reset() { v_.store(0); }

private:
    std::atomic<std::uint64_t> v_{0};
};

// A lock that never sleeps: test-and-test-and-set. The exchange claims the
// lock; while someone else holds it, spin on a plain LOAD (which can be
// served from a Shared copy of the line without stealing it) and only retry
// the exchange once it looks free. _mm_pause tells the core it is in a spin
// loop (yields pipeline resources to an SMT sibling, avoids a
// memory-order-violation flush on exit). Unlock is a release store — a plain
// `mov` on x86, no lock prefix.
class SpinlockCounter {
public:
    static constexpr const char* kName = "spinlock";
    std::uint64_t increment() {
        lock();
        ++v_;
        unlock();
        return 0;
    }
    std::uint64_t value() { return v_; }
    void reset() { v_ = 0; }

private:
    void lock() {
        while (locked_.exchange(true, std::memory_order_acquire)) {
            while (locked_.load(std::memory_order_relaxed)) _mm_pause();
        }
    }
    void unlock() { locked_.store(false, std::memory_order_release); }

    std::atomic<bool> locked_{false};
    std::uint64_t v_ = 0;
};

// The conventional answer: std::mutex (glibc pthread_mutex, default type).
// Uncontended, lock is one CAS and unlock one atomic exchange — no syscall.
// Contended, a thread that finds the lock taken marks it "has waiters" and
// SLEEPS in the kernel (futex_wait); the unlocker then pays a futex_wake
// syscall. Those sleeps are what race() counts as voluntary context switches.
class MutexCounter {
public:
    static constexpr const char* kName = "mutex";
    std::uint64_t increment() {
        std::lock_guard<std::mutex> guard(m_);
        ++v_;
        return 0;
    }
    std::uint64_t value() { return v_; }
    void reset() { v_ = 0; }

private:
    std::mutex m_;
    std::uint64_t v_ = 0;
};

// ---------------------------------------------------------------------------
// Private-counter layouts (Experiments 5 and 6)
// ---------------------------------------------------------------------------

// One counter per thread, all adjacent in one line-aligned block — Project
// 05's PackedLayout, generalized over the counter type. With 8-byte counters
// all kMaxThreads of them share ONE cache line (checked in exp1).
template <Counter C>
class PackedCounters {
public:
    C* at(std::size_t i) { return &slots_[i]; }

private:
    alignas(kCacheLineSize) C slots_[kMaxThreads]{};
};

// One counter per thread, each owning a whole line — Project 05's
// PaddedLayout. With FetchAddCounter this is also the standard fix for a
// contended shared counter: "sharding" — each thread increments its own
// slot, and a reader sums the slots.
template <Counter C>
struct alignas(kCacheLineSize) PaddedSlot {
    C counter{};
};

template <Counter C>
class PaddedCounters {
public:
    C* at(std::size_t i) { return &slots_[i].counter; }

private:
    PaddedSlot<C> slots_[kMaxThreads]{};
};

// Which cache line (address / 64) an object falls in.
inline std::uintptr_t line_of(const void* p) { return reinterpret_cast<std::uintptr_t>(p) / kCacheLineSize; }

// Per-thread counter pointers for race(): every thread on the SAME counter...
template <Counter C>
std::vector<C*> shared_by(C& counter, std::size_t n) {
    return std::vector<C*>(n, &counter);
}

// ...or thread i on slot i of a layout.
template <class Layout>
auto one_each(Layout& layout, std::size_t n) {
    std::vector<decltype(layout.at(0))> counters;
    for (std::size_t i = 0; i < n; ++i) counters.push_back(layout.at(i));
    return counters;
}

// ---------------------------------------------------------------------------
// Topology / pinning — same helpers as 05_false_sharing/src/false_sharing.hpp
// ---------------------------------------------------------------------------

// Physical core id of a logical CPU, from sysfs. Returns -1 if unreadable.
inline int physical_core_of(int logical_cpu) {
    std::ifstream f("/sys/devices/system/cpu/cpu" + std::to_string(logical_cpu) + "/topology/core_id");
    int core = -1;
    if (f) f >> core;
    return core;
}

// The lowest-numbered logical CPU of each physical core, skipping core 0, so
// a thread-count sweep never puts two threads on SMT siblings.
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
    double slowest_ns_per_increment = 0.0;  // the race ends when the slowest thread does
    double fastest_ns_per_increment = 0.0;  // a big gap to slowest = the counter was not shared fairly
    std::uint64_t cas_failures = 0;         // summed over threads (CasCounter only)
    long voluntary_switches = 0;            // summed over threads, timed loop only: the thread SLEPT
    long involuntary_switches = 0;          // summed over threads, timed loop only: the thread was preempted
    double user_seconds = 0.0;              // summed over threads, timed loop only
    double kernel_seconds = 0.0;            // summed over threads, timed loop only: syscalls (futex) run here
    std::uint64_t expected_total = 0;       // threads x iterations
    std::uint64_t observed_total = 0;       // sum of every distinct counter afterwards
    bool pinned_ok = true;                  // every thread landed on its requested CPU

    bool counts_ok() const { return observed_total == expected_total; }
    std::size_t threads() const { return ns_per_increment.size(); }
    // Total increments completed per microsecond across all threads
    // (= millions per second). All threads start together, so the slowest
    // thread's loop time is the wall time of the race.
    double aggregate_mops() const { return static_cast<double>(threads()) / slowest_ns_per_increment * 1e3; }
    // Share of the threads' CPU time spent in the kernel during the loop.
    // A pure user-space strategy should be ~0; a mutex that calls futex()
    // shows up here even when it never actually sleeps.
    double kernel_fraction() const { return kernel_seconds / (user_seconds + kernel_seconds); }
};

struct WorkerStats {
    double ns_per_increment = 0.0;
    std::uint64_t failures = 0;
    long voluntary_switches = 0;
    long involuntary_switches = 0;
    double user_seconds = 0.0;
    double kernel_seconds = 0.0;
    bool pinned = false;
};

inline double seconds_between(const timeval& a, const timeval& b) {
    return static_cast<double>(b.tv_sec - a.tv_sec) + static_cast<double>(b.tv_usec - a.tv_usec) * 1e-6;
}

// One racing thread: pin, check in at the barrier, spin until released, then
// time `iterations` increments of its counter. getrusage(RUSAGE_THREAD) is
// read just outside the timed region, so the context-switch counts and the
// user/kernel CPU split cover exactly the loop (and cost nothing inside it).
template <Counter C>
void run_race_worker(C* counter, int cpu, std::uint64_t iterations, std::atomic<std::size_t>& ready,
                     std::atomic<bool>& go, WorkerStats& out) {
    out.pinned = pin_current_thread(cpu) && sched_getcpu() == cpu;
    ready.fetch_add(1, std::memory_order_acq_rel);
    while (!go.load(std::memory_order_acquire)) {
    }

    rusage before{};
    getrusage(RUSAGE_THREAD, &before);
    std::uint64_t failures = 0;
    auto t0 = std::chrono::steady_clock::now();
    for (std::uint64_t k = 0; k < iterations; ++k) {
        failures += counter->increment();
    }
    auto t1 = std::chrono::steady_clock::now();
    rusage after{};
    getrusage(RUSAGE_THREAD, &after);

    out.ns_per_increment = std::chrono::duration<double, std::nano>(t1 - t0).count() / static_cast<double>(iterations);
    out.failures = failures;
    out.voluntary_switches = after.ru_nvcsw - before.ru_nvcsw;
    out.involuntary_switches = after.ru_nivcsw - before.ru_nivcsw;
    out.user_seconds = seconds_between(before.ru_utime, after.ru_utime);
    out.kernel_seconds = seconds_between(before.ru_stime, after.ru_stime);
}

// Sole responsibility: start one thread per entry in cpus, thread i pinned
// to cpus[i] and incrementing *counters[i], release them all at once, and
// time each thread's loop. counters may all point at ONE counter (true
// sharing) or at distinct ones (private) — the runner does not care, it just
// checks that the distinct counters sum to threads x iterations afterwards.
//
// As in Project 05, the loop is timed as a whole and divided: an
// uncontended increment is well under steady_clock's 10 ns resolution
// (Project 02). Thread creation, pinning and the start barrier are outside
// the timed region.
template <Counter C>
RaceResult race(const std::vector<C*>& counters, const std::vector<int>& cpus, std::uint64_t iterations) {
    const std::size_t n = cpus.size();
    RaceResult result;
    result.ns_per_increment.assign(n, 0.0);
    result.expected_total = n * iterations;
    if (n == 0 || counters.size() != n) {
        result.pinned_ok = false;
        return result;
    }

    std::vector<C*> distinct(counters);
    std::sort(distinct.begin(), distinct.end());
    distinct.erase(std::unique(distinct.begin(), distinct.end()), distinct.end());
    for (C* c : distinct) c->reset();

    std::atomic<std::size_t> ready{0};
    std::atomic<bool> go{false};
    std::vector<WorkerStats> stats(n);
    std::vector<std::thread> threads;
    threads.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        threads.emplace_back(run_race_worker<C>, counters[i], cpus[i], iterations, std::ref(ready), std::ref(go),
                             std::ref(stats[i]));
    }

    // Start barrier: wait until every worker is pinned and spinning, then
    // release them together so the timed loops genuinely overlap.
    while (ready.load(std::memory_order_acquire) < n) std::this_thread::yield();
    go.store(true, std::memory_order_release);
    for (auto& t : threads) t.join();

    result.fastest_ns_per_increment = stats[0].ns_per_increment;
    for (std::size_t i = 0; i < n; ++i) {
        result.ns_per_increment[i] = stats[i].ns_per_increment;
        result.slowest_ns_per_increment = std::max(result.slowest_ns_per_increment, stats[i].ns_per_increment);
        result.fastest_ns_per_increment = std::min(result.fastest_ns_per_increment, stats[i].ns_per_increment);
        result.cas_failures += stats[i].failures;
        result.voluntary_switches += stats[i].voluntary_switches;
        result.involuntary_switches += stats[i].involuntary_switches;
        result.user_seconds += stats[i].user_seconds;
        result.kernel_seconds += stats[i].kernel_seconds;
        result.pinned_ok &= stats[i].pinned;
    }
    for (C* c : distinct) result.observed_total += c->value();
    return result;
}

// ---------------------------------------------------------------------------
// Trials
// ---------------------------------------------------------------------------

// As in Project 05: each configuration is raced kTrials times inside one
// process after kWarmupRaces discarded races, and reported as min / p50 / max
// across trials (with ~10 samples p95+ would just be the max).
constexpr int kTrials = 11;
constexpr int kWarmupRaces = 1;

struct TrialStats {
    double min = 0.0;
    double p50 = 0.0;
    double max = 0.0;
};

inline TrialStats summarize_trials(std::vector<double> samples) {
    std::sort(samples.begin(), samples.end());
    return {samples.front(), samples[samples.size() / 2], samples.back()};
}

struct Measurement {
    TrialStats slowest;                     // slowest-thread ns/increment
    TrialStats fastest;                     // fastest-thread ns/increment
    TrialStats aggregate_mops;              // total M increments/s
    double cas_failures_per_increment = 0;  // over all timed trials
    double voluntary_switches_per_trial = 0;
    double involuntary_switches_per_trial = 0;
    double kernel_fraction = 0;  // kernel / (user + kernel) CPU time, over all timed trials
    std::vector<double> per_thread_p50;  // thread i's ns/increment, p50 across timed trials
    bool counts_ok = true;  // every race, warmup included
    bool pinned_ok = true;  // every race, warmup included

    bool ok() const { return counts_ok && pinned_ok; }
};

// Folds a set of races of ONE configuration into a Measurement. warmups only
// contribute their correctness checks; timed contribute everything.
inline Measurement summarize(const std::vector<RaceResult>& warmups, const std::vector<RaceResult>& timed) {
    Measurement m;
    for (const RaceResult& r : warmups) {
        m.counts_ok &= r.counts_ok();
        m.pinned_ok &= r.pinned_ok;
    }
    std::vector<double> slowest, fastest, aggregate;
    std::uint64_t failures = 0, increments = 0;
    long voluntary = 0, involuntary = 0;
    double user = 0.0, kernel = 0.0;
    for (const RaceResult& r : timed) {
        m.counts_ok &= r.counts_ok();
        m.pinned_ok &= r.pinned_ok;
        slowest.push_back(r.slowest_ns_per_increment);
        fastest.push_back(r.fastest_ns_per_increment);
        aggregate.push_back(r.aggregate_mops());
        failures += r.cas_failures;
        increments += r.expected_total;
        voluntary += r.voluntary_switches;
        involuntary += r.involuntary_switches;
        user += r.user_seconds;
        kernel += r.kernel_seconds;
    }
    m.slowest = summarize_trials(slowest);
    m.fastest = summarize_trials(fastest);
    m.aggregate_mops = summarize_trials(aggregate);
    m.cas_failures_per_increment = static_cast<double>(failures) / static_cast<double>(increments);
    m.voluntary_switches_per_trial = static_cast<double>(voluntary) / static_cast<double>(timed.size());
    m.involuntary_switches_per_trial = static_cast<double>(involuntary) / static_cast<double>(timed.size());
    m.kernel_fraction = kernel / (user + kernel);
    for (std::size_t i = 0; i < timed.front().threads(); ++i) {
        std::vector<double> thread_i;
        for (const RaceResult& r : timed) thread_i.push_back(r.ns_per_increment[i]);
        m.per_thread_p50.push_back(summarize_trials(thread_i).p50);
    }
    return m;
}

// Warmup + `trials` timed races of one configuration, back to back.
template <Counter C>
Measurement measure(const std::vector<C*>& counters, const std::vector<int>& cpus, std::uint64_t iterations,
                    int trials = kTrials) {
    std::vector<RaceResult> warmups, timed;
    for (int w = 0; w < kWarmupRaces; ++w) warmups.push_back(race(counters, cpus, iterations));
    for (int t = 0; t < trials; ++t) timed.push_back(race(counters, cpus, iterations));
    return summarize(warmups, timed);
}

// A named, type-erased configuration, for experiments that interleave
// several counter types trial by trial in one run (Experiments 4-6). The
// std::function is called once per RACE, never per increment, so it costs
// nothing inside the timed loop — race<C>() underneath is still fully typed.
struct Contender {
    std::string name;
    std::function<RaceResult()> run_once;
};

// Every thread in cpus on ONE counter, labelled with the counter's name.
template <Counter C>
Contender shared_contender(C& counter, const std::vector<int>& cpus, std::uint64_t iterations,
                           std::string name = C::kName) {
    return {std::move(name),
            [&counter, cpus, iterations] { return race(shared_by(counter, cpus.size()), cpus, iterations); }};
}

// Thread i in cpus on slot i of a private-counter layout.
template <class Layout>
Contender private_contender(Layout& layout, const std::vector<int>& cpus, std::uint64_t iterations,
                            std::string name) {
    return {std::move(name),
            [&layout, cpus, iterations] { return race(one_each(layout, cpus.size()), cpus, iterations); }};
}

// Races every contender kWarmupRaces times, then `trials` rounds in which
// each contender races once, the order rotating by one each round so no
// contender systematically gets the warmest (or coldest) machine. Returns
// one Measurement per contender, in the order given.
inline std::vector<Measurement> measure_interleaved(const std::vector<Contender>& contenders, int trials = kTrials) {
    const std::size_t k = contenders.size();
    std::vector<std::vector<RaceResult>> warmups(k), timed(k);
    for (int w = 0; w < kWarmupRaces; ++w)
        for (std::size_t i = 0; i < k; ++i) warmups[i].push_back(contenders[i].run_once());
    for (int t = 0; t < trials; ++t)
        for (std::size_t j = 0; j < k; ++j) {
            std::size_t i = (j + static_cast<std::size_t>(t)) % k;
            timed[i].push_back(contenders[i].run_once());
        }
    std::vector<Measurement> out;
    for (std::size_t i = 0; i < k; ++i) out.push_back(summarize(warmups[i], timed[i]));
    return out;
}

// Default placement for the two-thread experiments, same as Project 05: two
// distinct physical cores (CPU 2 = core 1, CPU 4 = core 2), away from CPU 0.
constexpr int kDefaultCpuA = 2;
constexpr int kDefaultCpuB = 4;
