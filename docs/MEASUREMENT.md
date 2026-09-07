# Measurement methodology

The repo's value is in its numbers. Numbers produced carelessly are worse than
none, because they teach the wrong lesson. These rules apply from Project 01.

## Clocks

- `std::chrono::steady_clock` for durations. Never `system_clock` (it can jump).
- `high_resolution_clock` is an alias for one of the two — don't use it, be explicit.
- Record the clock's actual resolution once, in `docs/ENVIRONMENT.md`.
- `rdtsc`/`__rdtscp` only when a project is specifically about it, and only after
  establishing that the TSC is invariant and the frequency is known.

## Anatomy of a measurement

1. **Warmup** — discard the first N iterations (caches, branch predictors, page
   faults, CPU frequency ramp). State N.
2. **Repetitions** — one run is an anecdote. Repeat the whole run ≥5 times and
   report the spread across runs, not just within one run.
3. **Iterations** — enough that total runtime ≫ clock resolution and ≫ scheduler
   tick, unless per-iteration latency is itself the subject.
4. **Report distributions, not averages.** From Project 02 onward the standard
   report is: `min, p50, p95, p99, p99.9, max`. The mean hides exactly the tail
   behavior HFT cares about.

## Do not let the compiler delete your benchmark

`-O2` will remove work whose result is unused. Consume the result (accumulate it
and print it, or use an inline-asm "escape"). If a benchmark reports 0 ns, assume
it was optimized away until proven otherwise.

## Do not measure the harness

The timing code, the RNG, the formatting — measure the empty loop first and
subtract or acknowledge it.

## Change one thing

A run differs from the baseline in exactly one variable. Two changes at once
produce a number you cannot explain.

## Sources of noise to name explicitly

- Other processes; frequency scaling / turbo; SMT sibling activity
- Interrupts and timer ticks; page faults on first touch; NUMA placement
- Thermal throttling on long runs
- **WSL2**: a virtualized kernel with limited control over IRQs, isolation,
  huge pages, and no direct NIC access. Fine for stages 1–3; stages 4+ need
  native Linux. Record which environment produced every result.

## Honesty rules

- Never write a number you did not measure on the machine you name.
- A negative or null result is a result — record it.
- If run-to-run variance exceeds the effect being measured, the honest
  conclusion is "no measurable difference", not the direction you hoped for.
