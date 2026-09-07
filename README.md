# hft-lab

A long-term, incremental lab for low-latency systems engineering in C++ on Linux —
built as **72 small, independent, measurable projects** that are progressively harder
and are eventually composed into one complete HFT trading pipeline.

## The rule

> Every project teaches **one** low-latency concept, produces **measurable results**,
> and is **later reused** by the final system.

No project is "done" because the code compiles. A project is done when the numbers
are recorded and the *reason* for those numbers is written down.

## Repository layout

```
hft-lab/
├── README.md                  <- you are here
├── CMakeLists.txt             <- root build; auto-discovers NN_* project dirs
├── common/                    <- shared, reusable code (grows only when 2+ projects need it)
│   └── include/hft/
├── docs/
│   ├── ROADMAP.md             <- all 72 projects, stages 1-7
│   ├── CONVENTIONS.md         <- project structure, naming, build, benchmark rules
│   ├── README_TEMPLATE.md     <- the 11-section project README every project must fill
│   ├── MEASUREMENT.md         <- how to measure honestly (methodology, pitfalls)
│   ├── ENVIRONMENT.md         <- the machine(s) results are recorded on
│   ├── PROGRESS.md            <- 0/72 tracker
│   ├── templates/project_template/
│   └── reference/             <- original roadmap + per-project source specs
└── scripts/
    └── new_project.sh         <- scaffold the next project from the template
```

Project directories (`01_cpu_pinned_worker/`, `02_latency_timer/`, ...) are created
**one at a time**, only when that project is started. There are deliberately none yet.

## Each project directory

```
NN_project_name/
├── README.md      <- the write-up (see docs/README_TEMPLATE.md) — the real deliverable
├── CMakeLists.txt
├── src/           <- implementation
├── benchmark/     <- the harness that produces numbers
└── results/       <- raw output + notes, committed, never invented
```

## Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The root build discovers every `NN_*/CMakeLists.txt` automatically; nothing needs to
be registered by hand.

## Status

Stage 0 — repository architecture and planning. **No project implemented yet.**
Next action: Project 01 — CPU-Pinned Worker (spec in `docs/reference/`).

See [docs/ROADMAP.md](docs/ROADMAP.md) and [docs/PROGRESS.md](docs/PROGRESS.md).
