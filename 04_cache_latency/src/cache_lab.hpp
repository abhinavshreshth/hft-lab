// Shared pointer-chasing apparatus for Project 04's experiments. Like
// 02_latency_timer's latency_stats.hpp and 03_cpu_migration's
// cpu_topology.hpp, this is the fixed measurement instrument this project
// builds (a latency-bound memory-access chain, not a bandwidth test), shared
// via plain #include across every binary that needs it. No hft:: namespace —
// that prefix is reserved for code actually promoted to common/include/hft/
// (docs/CONVENTIONS.md §6), which this is not.
//
// Chain construction and chain traversal are split into two SOLID-shaped
// pieces, the same way 03_cpu_migration's CpuTopology depends on an
// injected CoreIdReader abstraction rather than reading sysfs itself:
//
//   ChainBuilder   — how the next-pointers are laid out (the experimental
//                    variable: sequential vs. random order).
//   PointerChaser  — how a chain, once built, is timed (fixed apparatus,
//                    identical for every access pattern).
//
// PointerChaser depends on the ChainBuilder interface, never on a concrete
// pattern (Dependency Inversion) — it only ever calls nodes[p].next, which
// every ChainBuilder guarantees is a valid index into the vector it
// returned. A new access pattern (e.g. fixed-stride) can be added as a new
// ChainBuilder subclass without touching PointerChaser or either existing
// builder (Open/Closed), and — because any ChainBuilder's output is
// interchangeable input to PointerChaser (Liskov Substitution) — an
// experiment picks its pattern by choosing which concrete builder to
// construct, at exactly one call site (see exp2/exp3's one-line diff).
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

// One node per cache line. alignas(64) forces the compiler to pad sizeof
// up to 64 as well as align the vector's backing storage to it, so N nodes
// occupy exactly N cache lines with nothing else sharing them — the buffer
// size in bytes is exactly N * 64.
struct alignas(64) Node {
    std::uint64_t next;
};
static_assert(sizeof(Node) == 64, "one Node must occupy exactly one cache line");

// The working-set sizes this project sweeps, chosen to straddle every
// boundary in docs/ENVIRONMENT.md's cache hierarchy (L1d 32K, L2 1M,
// L3 32M) and to land solidly beyond L3 into plain RAM at the top end.
inline const std::vector<std::size_t>& sweep_sizes_bytes() {
    static const std::vector<std::size_t> sizes = {
        4ull * 1024,          // L1
        16ull * 1024,         // L1
        32ull * 1024,         // L1/L2 boundary
        64ull * 1024,         // L2
        256ull * 1024,        // L2
        1ull * 1024 * 1024,   // L2/L3 boundary
        2ull * 1024 * 1024,   // L3
        8ull * 1024 * 1024,   // L3
        16ull * 1024 * 1024,  // L3
        32ull * 1024 * 1024,  // L3/RAM boundary
        64ull * 1024 * 1024,  // RAM
        128ull * 1024 * 1024, // RAM
    };
    return sizes;
}

inline std::size_t node_count(std::size_t bytes) { return bytes / sizeof(Node); }

// Builds an N-node chain whose .next fields form some traversal order over
// [0, n). The interface carries only what any experiment needs from a
// chain — build it, name it for output — so no builder is forced to
// implement anything it doesn't use (Interface Segregation).
class ChainBuilder {
public:
    virtual ~ChainBuilder() = default;
    virtual std::vector<Node> build(std::size_t n) const = 0;
    virtual const char* pattern_name() const = 0;
};

// Baseline access pattern: next[i] = (i + 1) % n. Visits cache lines in
// increasing address order — a constant unit-stride-in-cache-lines pattern
// a hardware prefetcher can detect and run ahead of. Sole responsibility:
// produce that one layout (Single Responsibility) — it knows nothing about
// timing.
class SequentialChainBuilder : public ChainBuilder {
public:
    std::vector<Node> build(std::size_t n) const override {
        std::vector<Node> nodes(n);
        for (std::size_t i = 0; i < n; ++i) {
            nodes[i].next = (i + 1) % n;
        }
        return nodes;
    }

    const char* pattern_name() const override { return "sequential"; }
};

// Modified access pattern: a single random N-cycle over the same N nodes,
// built with Sattolo's algorithm (not Fisher-Yates — Fisher-Yates can
// produce several disjoint short cycles, which would let the chase settle
// into a small, easily-prefetched or easily-cached loop instead of touching
// all N cache lines in random order). Sattolo guarantees exactly one cycle
// of length N with no fixed points, so following .next from any start
// visits every node exactly once before repeating, in an order a hardware
// prefetcher cannot predict. The seed is injected at construction rather
// than hardcoded, so a caller controls reproducibility without this class
// needing to change.
class RandomChainBuilder : public ChainBuilder {
public:
    explicit RandomChainBuilder(std::uint64_t seed) : seed_(seed) {}

    std::vector<Node> build(std::size_t n) const override {
        std::vector<std::size_t> order(n);
        for (std::size_t i = 0; i < n; ++i) order[i] = i;

        std::mt19937_64 rng(seed_);
        for (std::size_t i = n; i-- > 1;) {
            std::uniform_int_distribution<std::size_t> dist(0, i - 1);
            std::size_t j = dist(rng);
            std::swap(order[i], order[j]);
        }

        std::vector<Node> nodes(n);
        for (std::size_t i = 0; i < n; ++i) {
            nodes[order[i]].next = order[(i + 1) % n];
        }
        return nodes;
    }

    const char* pattern_name() const override { return "random"; }

private:
    std::uint64_t seed_;
};

// Returns false if the chain is not a single N-cycle (used by Experiment 1
// to validate RandomChainBuilder's output before trusting any latency it
// reports). A free function, not a ChainBuilder method: it is a property
// any chain can be checked against, not something only one builder needs.
inline bool is_single_cycle(const std::vector<Node>& nodes) {
    const std::size_t n = nodes.size();
    std::size_t p = 0;
    std::size_t visited = 0;
    do {
        p = nodes[p].next;
        ++visited;
    } while (p != 0 && visited <= n);
    return visited == n;
}

struct ChaseResult {
    double ns_per_access;
    std::size_t final_index;  // consumed by every caller so -O2 cannot delete the chase
};

// Sole responsibility: time a dependent-load walk over a chain someone
// else already built (Single Responsibility) — it depends only on the
// Node/vector shape every ChainBuilder produces, never on a concrete
// builder (Dependency Inversion), so it needs no change when a new access
// pattern is added.
//
// chase() runs warmup_steps untimed (bring the working set to whatever
// residency it will reach — full cache residency if it fits, otherwise
// just page-fault-in), then measure_steps timed. Each step depends on the
// previous step's loaded value, so the compiler cannot reorder, vectorize,
// or run steps ahead of each other — the elapsed time is a true
// dependent-load latency, not throughput.
class PointerChaser {
public:
    ChaseResult chase(std::vector<Node>& nodes, std::uint64_t warmup_steps,
                       std::uint64_t measure_steps) const {
        std::size_t p = 0;
        for (std::uint64_t s = 0; s < warmup_steps; ++s) {
            p = nodes[p].next;
        }

        auto t0 = std::chrono::steady_clock::now();
        for (std::uint64_t s = 0; s < measure_steps; ++s) {
            p = nodes[p].next;
        }
        auto t1 = std::chrono::steady_clock::now();

        double elapsed_ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
        return {elapsed_ns / static_cast<double>(measure_steps), p};
    }
};

constexpr std::uint64_t kMeasureSteps = 3'000'000;
constexpr std::uint64_t kRandomSeed = 0x04CACE1A7E4C7ULL;  // fixed for reproducibility

inline std::uint64_t warmup_steps_for(std::size_t n) {
    return 4ull * static_cast<std::uint64_t>(n);
}
