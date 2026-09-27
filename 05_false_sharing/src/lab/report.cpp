#include "report.hpp"

#include <iomanip>
#include <iostream>

#include "cache_line.hpp"
#include "cpu_placement.hpp"

namespace false_sharing::report {

namespace {

const char* checks_line(bool ok) { return ok ? "checks: ok\n" : "checks: FAILED (count or pinning)\n"; }

void cpu_with_core(int cpu) { std::cout << cpu << " (core " << physical_core_of(cpu) << ")"; }

void stats_row(const char* label, const TrialStats& s) {
    std::cout << std::setw(8) << label << std::setw(10) << s.min << std::setw(10) << s.p50 << std::setw(10) << s.max
              << "\n";
}

}  // namespace

void check(bool ok, const std::string& what) { std::cout << (ok ? "PASS" : "FAIL") << ": " << what << "\n"; }

void end_section() { std::cout << "\n"; }

// ---- Experiment 1 ----------------------------------------------------------

void sanity_header() { std::cout << "=== Experiment 1: apparatus sanity check ===\n\n"; }

void line_size(const LineSizeCheck& c) {
    std::cout << "-- cache line size --\n";
    std::cout << "  kCacheLineSize (this project):          " << c.project << "\n";
    if (c.std_library) {
        std::cout << "  hardware_destructive_interference_size: " << *c.std_library << "\n";
    } else {
        std::cout << "  hardware_destructive_interference_size: not provided by this standard library\n";
    }
    std::cout << "  sysfs coherency_line_size:              " << c.sysfs << "\n";
    check(c.ok, c.std_library ? "all three agree" : "kCacheLineSize and sysfs agree");
    end_section();
}

void layouts_header() { std::cout << "-- layouts (runtime addresses) --\n"; }

void layout_addresses(CounterLayout& layout, std::size_t n) {
    auto base = reinterpret_cast<std::uintptr_t>(layout.counter(0));
    std::cout << "  " << layout.layout_name() << ":  base % 64 = " << base % kCacheLineSize << "\n";
    for (std::size_t i = 0; i < n; ++i) {
        auto addr = reinterpret_cast<std::uintptr_t>(layout.counter(i));
        std::cout << "    counter " << i << "  offset " << std::setw(4) << addr - base << " B  line +"
                  << line_of(layout.counter(i)) - line_of(layout.counter(0)) << "\n";
    }
}

void topology(const std::vector<int>& one_cpu_per_core) {
    std::cout << "-- topology --\n";
    for (int cpu = 0; cpu < logical_cpu_count(); ++cpu) {
        std::cout << "  cpu " << std::setw(2) << cpu << " -> core " << physical_core_of(cpu)
                  << ((cpu % 4 == 3) ? "\n" : "   ");
    }
    std::cout << "  one_cpu_per_core():";
    for (int cpu : one_cpu_per_core) std::cout << " " << cpu;
    std::cout << "\n\n";
}

void single_thread(int cpu, std::uint64_t iterations, const Comparison& c) {
    std::cout << "-- single thread: layout alone must not matter --\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "  cpu " << cpu << ", " << iterations << " increments, " << kTrials << " trials\n";
    std::cout << "  packed ns/increment  min " << c.a.stats.min << "  p50 " << c.a.stats.p50 << "  max "
              << c.a.stats.max << "\n";
    std::cout << "  padded ns/increment  min " << c.b.stats.min << "  p50 " << c.b.stats.p50 << "  max "
              << c.b.stats.max << "\n";
    std::cout << "  packed/padded p50:   " << c.p50_ratio() << "\n";
    check(c.ok(), "every counter reached " + std::to_string(iterations) + " and every thread ran on its pinned CPU");
    end_section();
}

void sanity_verdict(bool pass) { std::cout << (pass ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED") << "\n"; }

// ---- Experiments 2 and 3 ---------------------------------------------------

void two_thread_run(const std::string& experiment, CounterLayout& layout, const std::vector<int>& cpus,
                    std::uint64_t iterations, const Measurement& m) {
    bool same = line_of(layout.counter(0)) == line_of(layout.counter(1));
    std::cout << "=== " << experiment << ": " << layout.layout_name() << " counters, 2 threads ===\n";
    std::cout << "cpus ";
    cpu_with_core(cpus[0]);
    std::cout << " and ";
    cpu_with_core(cpus[1]);
    std::cout << ", " << iterations << " increments per thread, " << kTrials << " trials\n";
    std::cout << "counter lines: " << line_of(layout.counter(0)) << ", " << line_of(layout.counter(1))
              << (same ? "  (SAME line)\n" : "  (different lines)\n");
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "slowest-thread ns/increment  min " << m.stats.min << "  p50 " << m.stats.p50 << "  max "
              << m.stats.max << "\n";
    std::cout << checks_line(m.ok);
}

// ---- Experiment 4 ----------------------------------------------------------

void same_run_comparison(const std::vector<int>& cpus, std::uint64_t iterations, const Comparison& c) {
    std::cout << "=== Experiment 4: packed vs padded, same run ===\n";
    std::cout << "cpus ";
    cpu_with_core(cpus[0]);
    std::cout << " and ";
    cpu_with_core(cpus[1]);
    std::cout << ", " << iterations << " increments per thread, " << kTrials << " trials each after " << kWarmupRaces
              << " discarded warmup race(s), order alternating\n\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << std::setw(8) << "layout" << std::setw(10) << "min" << std::setw(10) << "p50" << std::setw(10)
              << "max" << "   (slowest-thread ns/increment)\n";
    stats_row("packed", c.a.stats);
    stats_row("padded", c.b.stats);
    std::cout << "packed/padded p50: " << c.p50_ratio() << "x\n";
    std::cout << checks_line(c.ok());
}

// ---- Experiment 5 ----------------------------------------------------------

void not_enough_cores(std::size_t needed, std::size_t found) {
    std::cerr << "need " << needed << " distinct physical cores, found " << found << "\n";
}

void thread_scaling(const std::vector<int>& cpus, std::uint64_t iterations, const std::vector<ScalingPoint>& points,
                    bool ok) {
    std::cout << "=== Experiment 5: thread-count scaling ===\n";
    std::cout << "cpus (one per physical core, in order used):";
    for (int c : cpus) std::cout << " " << c;
    std::cout << "\n" << iterations << " increments per thread, " << kTrials << " trials per point\n\n";

    std::cout << std::fixed << std::setprecision(3);
    std::cout << std::setw(8) << "threads" << std::setw(12) << "padded p50" << std::setw(12) << "packed p50"
              << std::setw(12) << "packed min" << std::setw(12) << "packed max" << std::setw(10) << "ratio"
              << "   (slowest-thread ns/increment)\n";
    for (const auto& pt : points) {
        const TrialStats& p = pt.packed_vs_padded.a.stats;
        const TrialStats& q = pt.packed_vs_padded.b.stats;
        std::cout << std::setw(8) << pt.threads << std::setw(12) << q.p50 << std::setw(12) << p.p50 << std::setw(12)
                  << p.min << std::setw(12) << p.max << std::setw(9) << pt.packed_vs_padded.p50_ratio() << "x\n";
    }
    std::cout << "\n" << checks_line(ok);
}

// ---- Experiment 6 ----------------------------------------------------------

void offset_sweep(const std::vector<int>& cpus, std::uint64_t iterations, const std::vector<OffsetPoint>& points,
                  bool ok) {
    std::cout << "=== Experiment 6: offset sweep ===\n";
    std::cout << "cpus " << cpus[0] << " and " << cpus[1] << ", " << iterations << " increments per thread, "
              << kTrials << " trials per offset\n\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << std::setw(8) << "offset" << std::setw(11) << "same line" << std::setw(10) << "min" << std::setw(10)
              << "p50" << std::setw(10) << "max" << "   (slowest-thread ns/increment)\n";
    for (const auto& pt : points) {
        const TrialStats& s = pt.measurement.stats;
        std::cout << std::setw(6) << pt.offset_bytes << " B" << std::setw(11) << (pt.same_line ? "yes" : "no")
                  << std::setw(10) << s.min << std::setw(10) << s.p50 << std::setw(10) << s.max << "\n";
    }
    std::cout << "\n" << checks_line(ok);
}

// ---- Experiment 7 ----------------------------------------------------------

void placement_sweep(int anchor_cpu, std::uint64_t iterations, const std::vector<PlacementPoint>& points, bool ok) {
    std::cout << "=== Experiment 7: placement sweep ===\n";
    std::cout << "anchor cpu ";
    cpu_with_core(anchor_cpu);
    std::cout << ", " << iterations << " increments per thread, " << kTrials << " trials per point\n\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << std::setw(8) << "partner" << std::setw(6) << "core" << std::setw(13) << "relation" << std::setw(12)
              << "padded p50" << std::setw(12) << "packed p50" << std::setw(12) << "packed max" << std::setw(10)
              << "ratio" << "   (slowest-thread ns/increment)\n";
    for (const auto& pt : points) {
        const TrialStats& p = pt.packed_vs_padded.a.stats;
        const TrialStats& q = pt.packed_vs_padded.b.stats;
        std::cout << std::setw(8) << pt.partner_cpu << std::setw(6) << physical_core_of(pt.partner_cpu)
                  << std::setw(13) << (pt.smt_sibling ? "smt-sibling" : "other-core") << std::setw(12) << q.p50
                  << std::setw(12) << p.p50 << std::setw(12) << p.max << std::setw(9)
                  << pt.packed_vs_padded.p50_ratio() << "x\n";
    }
    std::cout << "\n" << checks_line(ok);
}

}  // namespace false_sharing::report
