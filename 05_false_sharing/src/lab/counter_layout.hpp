// WHERE each thread's counter lives — the experimental variable.
//
// Mirrors 04_cache_latency's ChainBuilder: RaceRunner depends only on the
// CounterLayout interface, so an experiment picks its layout by constructing
// one concrete class at one call site (the exp2 -> exp3 diff is that line).
#pragma once

#include <cstddef>
#include <cstdint>

#include "cache_line.hpp"

namespace false_sharing {

// volatile: every increment must be a real load + store (not hoisted into a
// register by -O2), because the store is what pulls the line into this core
// in Modified state. Not std::atomic: each counter has exactly one writer, so
// there is no race to make atomic — the threads share only the line.
class CounterLayout {
public:
    virtual ~CounterLayout() = default;
    virtual volatile std::uint64_t* counter(std::size_t thread_index) = 0;
    virtual std::size_t max_threads() const = 0;
    virtual const char* layout_name() const = 0;
};

// Baseline: all counters adjacent in one line-aligned block — what
// `std::uint64_t counters[N];` gives you by default.
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

// The standard fix: alignas(64) on the element type pads sizeof to 64 and
// aligns each element to a line boundary, so each counter owns a whole line.
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

// Two counters offset_bytes apart inside a line-aligned buffer, for the
// offset sweep. offset_bytes must be a non-zero multiple of 8 (two uint64_t
// cannot overlap) and <= kMaxOffsetBytes.
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

}  // namespace false_sharing
