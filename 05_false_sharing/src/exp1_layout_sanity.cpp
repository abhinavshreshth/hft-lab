// Experiment 1 — Apparatus sanity check.
//
// Before trusting any timing from Experiments 2-7, this validates that the
// apparatus is what it claims to be, the same role Experiment 1 played in
// Projects 02-04:
//
// 1. The cache-line size this project hardcodes (kCacheLineSize) agrees with
//    what the compiler (std::hardware_destructive_interference_size, when the
//    standard library provides it) and the kernel (sysfs
//    coherency_line_size) say it is.
// 2. PackedLayout really puts all 8 counters in ONE cache line, PaddedLayout
//    really gives each counter its OWN line, and OffsetLayout(k) puts its two
//    counters in the same line exactly when k < 64 — checked from the actual
//    runtime addresses, not assumed from the struct definitions.
// 3. The topology the multi-thread experiments place threads on: which
//    logical CPUs are SMT siblings, and which CPUs one_cpu_per_core() picks.
// 4. With ONE thread there is nobody to share with, so packed and padded
//    must cost the same per increment. If they don't, the layout itself (not
//    sharing) is changing the cost, and every later comparison is suspect.
// 5. Every race leaves every counter at exactly `iterations` and every
//    thread on its requested CPU (Measurement::ok).

#include "lab/cpu_placement.hpp"
#include "lab/report.hpp"
#include "lab/sanity.hpp"
#include "lab/trials.hpp"

using namespace false_sharing;

constexpr std::uint64_t kIterations = 50'000'000;

int main() {
    report::sanity_header();
    bool pass = true;

    // 1. Line size: project constant vs compiler vs kernel.
    LineSizeCheck line = check_line_size();
    report::line_size(line);
    pass &= line.ok;

    // 2. Layouts, from the runtime addresses.
    PackedLayout packed;
    PaddedLayout padded;
    report::layouts_header();
    report::layout_addresses(packed, kMaxThreads);
    report::layout_addresses(padded, kMaxThreads);

    bool packed_ok = all_in_one_line(packed, kMaxThreads);
    bool padded_ok = all_in_distinct_lines(padded, kMaxThreads);
    bool offset_ok = offset_layout_matches_line_size();
    report::check(packed_ok, "packed puts all " + std::to_string(kMaxThreads) + " counters in one line");
    report::check(padded_ok, "padded puts each counter in its own line");
    report::check(offset_ok, "offset layout shares a line exactly when offset < 64 (8..512 B)");
    report::end_section();
    pass &= packed_ok && padded_ok && offset_ok;

    // 3. Topology the multi-thread experiments place threads on.
    report::topology(one_cpu_per_core());

    // 4 + 5. One thread: nobody to share with, so layout must not matter;
    // every race must also pass its count and pinning checks.
    Comparison single = measure_both(packed, padded, {kDefaultCpuA}, kIterations, /*a_first=*/true);
    report::single_thread(kDefaultCpuA, kIterations, single);
    pass &= single.ok();

    report::sanity_verdict(pass);
    return pass ? 0 : 1;
}
