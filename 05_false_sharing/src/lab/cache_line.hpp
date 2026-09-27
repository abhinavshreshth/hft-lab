// The one hardware fact this whole project is about: coherence works on
// 64-byte lines, not on variables.
#pragma once

#include <cstddef>
#include <cstdint>

namespace false_sharing {

// sysfs coherency_line_size on this machine. Hardcoded rather than taken from
// std::hardware_destructive_interference_size so the value under test is
// visible in the source; Experiment 1 checks the two agree.
constexpr std::size_t kCacheLineSize = 64;

// 8 x uint64_t fill one line exactly, so up to 8 threads can all be packed
// into a single line — the upper bound of every thread-count sweep.
constexpr std::size_t kMaxThreads = kCacheLineSize / sizeof(std::uint64_t);

// Which cache line (address / 64) an address falls in.
inline std::uintptr_t line_of(const volatile void* p) {
    return reinterpret_cast<std::uintptr_t>(p) / kCacheLineSize;
}

}  // namespace false_sharing
