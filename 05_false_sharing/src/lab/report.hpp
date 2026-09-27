// All console output for Project 05. The experiments compute; this module
// decides how results look. Keeping the format here (and only here) is what
// lets benchmark/run.sh captures stay comparable when an experiment changes.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "counter_layout.hpp"
#include "sanity.hpp"
#include "trials.hpp"

namespace false_sharing::report {

// Rows the sweep experiments hand over once they have finished measuring.
struct ScalingPoint {
    std::size_t threads;
    Comparison packed_vs_padded;
};

struct OffsetPoint {
    std::size_t offset_bytes;
    bool same_line;
    Measurement measurement;
};

struct PlacementPoint {
    int partner_cpu;
    bool smt_sibling;
    Comparison packed_vs_padded;
};

// Shared pieces.
void check(bool ok, const std::string& what);  // "PASS: <what>" / "FAIL: <what>"
void end_section();

// Experiment 1.
void sanity_header();
void line_size(const LineSizeCheck& c);
void layouts_header();
void layout_addresses(CounterLayout& layout, std::size_t n);
void topology(const std::vector<int>& one_cpu_per_core);
void single_thread(int cpu, std::uint64_t iterations, const Comparison& packed_vs_padded);
void sanity_verdict(bool pass);

// Experiments 2 and 3.
void two_thread_run(const std::string& experiment, CounterLayout& layout, const std::vector<int>& cpus,
                    std::uint64_t iterations, const Measurement& m);

// Experiment 4.
void same_run_comparison(const std::vector<int>& cpus, std::uint64_t iterations, const Comparison& packed_vs_padded);

// Experiment 5.
void not_enough_cores(std::size_t needed, std::size_t found);
void thread_scaling(const std::vector<int>& cpus, std::uint64_t iterations, const std::vector<ScalingPoint>& points,
                    bool ok);

// Experiment 6.
void offset_sweep(const std::vector<int>& cpus, std::uint64_t iterations, const std::vector<OffsetPoint>& points,
                  bool ok);

// Experiment 7.
void placement_sweep(int anchor_cpu, std::uint64_t iterations, const std::vector<PlacementPoint>& points, bool ok);

}  // namespace false_sharing::report
