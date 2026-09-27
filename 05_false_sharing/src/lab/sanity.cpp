#include "sanity.hpp"

#include <fstream>
#include <new>
#include <set>

namespace false_sharing {

LineSizeCheck check_line_size() {
    LineSizeCheck c;
    c.project = kCacheLineSize;
    std::ifstream("/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size") >> c.sysfs;
    c.ok = c.sysfs == static_cast<int>(kCacheLineSize);
    // The standard library only declares this constant when the compiler
    // supplies a value for it (libstdc++ keys it off g++'s predefined
    // __GCC_DESTRUCTIVE_SIZE), so it is guarded by its feature-test macro.
    // g++ 15 defines it; IntelliSense's parser and older libc++ do not.
#ifdef __cpp_lib_hardware_interference_size
    c.std_library = std::hardware_destructive_interference_size;
    c.ok &= *c.std_library == kCacheLineSize;
#endif
    return c;
}

bool all_in_one_line(CounterLayout& layout, std::size_t n) {
    for (std::size_t i = 1; i < n; ++i)
        if (line_of(layout.counter(i)) != line_of(layout.counter(0))) return false;
    return true;
}

bool all_in_distinct_lines(CounterLayout& layout, std::size_t n) {
    std::set<std::uintptr_t> lines;
    for (std::size_t i = 0; i < n; ++i) lines.insert(line_of(layout.counter(i)));
    return lines.size() == n;
}

bool offset_layout_matches_line_size() {
    for (std::size_t off = 8; off <= OffsetLayout::kMaxOffsetBytes; off += 8) {
        OffsetLayout layout(off);
        if (all_in_one_line(layout, 2) != (off < kCacheLineSize)) return false;
    }
    return true;
}

}  // namespace false_sharing
