#include "trials.hpp"

#include <algorithm>

#include "race_runner.hpp"

namespace false_sharing {

TrialStats summarize_trials(std::vector<double> samples) {
    std::sort(samples.begin(), samples.end());
    return {samples.front(), samples[samples.size() / 2], samples.back()};
}

Measurement measure(CounterLayout& layout, const std::vector<int>& cpus, std::uint64_t iterations, int trials) {
    RaceRunner runner;
    Measurement m;
    for (int w = 0; w < kWarmupRaces; ++w) {
        RaceResult r = runner.run(layout, cpus, iterations);
        m.ok &= r.counts_ok && r.pinned_ok;
    }
    std::vector<double> samples;
    for (int t = 0; t < trials; ++t) {
        RaceResult r = runner.run(layout, cpus, iterations);
        m.ok &= r.counts_ok && r.pinned_ok;
        samples.push_back(r.slowest_ns_per_increment);
    }
    m.stats = summarize_trials(samples);
    return m;
}

Comparison measure_both(CounterLayout& a, CounterLayout& b, const std::vector<int>& cpus, std::uint64_t iterations,
                        bool a_first) {
    Comparison c;
    if (a_first) {
        c.a = measure(a, cpus, iterations);
        c.b = measure(b, cpus, iterations);
    } else {
        c.b = measure(b, cpus, iterations);
        c.a = measure(a, cpus, iterations);
    }
    return c;
}

Comparison measure_interleaved(CounterLayout& a, CounterLayout& b, const std::vector<int>& cpus,
                               std::uint64_t iterations) {
    RaceRunner runner;
    Comparison c;
    for (CounterLayout* layout : {&a, &b}) {
        Measurement& m = (layout == &a) ? c.a : c.b;
        for (int w = 0; w < kWarmupRaces; ++w) {
            RaceResult r = runner.run(*layout, cpus, iterations);
            m.ok &= r.counts_ok && r.pinned_ok;
        }
    }

    std::vector<double> a_ns, b_ns;
    for (int t = 0; t < kTrials; ++t) {
        CounterLayout* order[2] = {&a, &b};
        if (t % 2 != 0) std::swap(order[0], order[1]);
        for (CounterLayout* layout : order) {
            RaceResult r = runner.run(*layout, cpus, iterations);
            Measurement& m = (layout == &a) ? c.a : c.b;
            m.ok &= r.counts_ok && r.pinned_ok;
            (layout == &a ? a_ns : b_ns).push_back(r.slowest_ns_per_increment);
        }
    }
    c.a.stats = summarize_trials(a_ns);
    c.b.stats = summarize_trials(b_ns);
    return c;
}

}  // namespace false_sharing
