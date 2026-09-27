// The checks Experiment 1 runs on the apparatus before any timing is trusted.
#pragma once

#include <cstddef>
#include <optional>

#include "counter_layout.hpp"

namespace false_sharing {

struct LineSizeCheck {
    std::size_t project = 0;                  // kCacheLineSize
    std::optional<std::size_t> std_library;   // hardware_destructive_interference_size, if provided
    int sysfs = -1;                           // coherency_line_size
    bool ok = false;                          // every available source agrees
};

LineSizeCheck check_line_size();

// From the actual runtime addresses, not the struct definitions.
bool all_in_one_line(CounterLayout& layout, std::size_t n);
bool all_in_distinct_lines(CounterLayout& layout, std::size_t n);

// OffsetLayout(k) shares a line exactly when k < 64, for every k in 8..512.
bool offset_layout_matches_line_size();

}  // namespace false_sharing
