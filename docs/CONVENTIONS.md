# Conventions

Rules that keep 72 projects consistent enough to compare and reuse.

## 1. One project = one concept

If a project needs two new concepts to work, one of them belongs in an earlier
project. Split it rather than growing it.

## 2. Directory layout

```
NN_project_name/
├── README.md        # the write-up — see docs/README_TEMPLATE.md
├── CMakeLists.txt
├── src/             # implementation
├── benchmark/       # the harness producing the numbers
└── results/         # raw output + notes (committed)
```

- `NN` is zero-padded and matches `docs/ROADMAP.md` exactly.
- Directory names are lowercase `snake_case`, fixed by the roadmap table.
- Create a project directory only when starting that project.

## 3. Naming

| Thing | Style | Example |
|---|---|---|
| Directory | `snake_case` | `08_spsc_queue` |
| Header/source | `snake_case` | `spsc_queue.hpp` |
| Type | `PascalCase` | `SpscQueue` |
| Function / variable | `snake_case` | `push_back`, `cpu_id` |
| Constant | `kPascalCase` | `kCacheLineSize` |
| Namespace | `hft`, then `hft::<area>` | `hft::bench` |
| Executable target | `NN_name` | `01_cpu_pinned_worker` |

## 4. Build

- C++20, `-std=c++20` (no GNU extensions unless a project documents why).
- Release = `-O2 -g`. Symbols stay in so `perf` is usable.
- Each project has its own `CMakeLists.txt`; the root build discovers it via glob.
- Every target links `hft_common` (warnings + shared headers + pthread).
- Warnings: `-Wall -Wextra -Wpedantic`. Fix them; don't silence them.

Minimal per-project `CMakeLists.txt`:

```cmake
add_executable(NN_project_name src/main.cpp)
target_link_libraries(NN_project_name PRIVATE hft_common)
```

## 5. Dependencies

Default answer is **no**. The standard library plus Linux system APIs cover
stages 1–4 entirely. A dependency is allowed only when it *is* the subject of the
project (libbpf for stage 5 XDP/AF_XDP, DPDK for 46+), and it must be:

- confined to the projects that need it,
- guarded in CMake so the rest of the repo still builds without it,
- recorded in that project's README under Environment.

No Boost. No test framework, no benchmark framework, no logging library —
writing the harness *is* part of the learning.

## 6. `common/`

`common/include/hft/` holds code only after **two or more** projects need it.
The path into `common/` is: write it in the project → a later project needs it →
*then* promote it, with the promoting project noted in a header comment.
Nothing is written speculatively.

## 7. Results are data, not decoration

- Every number in a README comes from a run on the machine described in
  `docs/ENVIRONMENT.md`. Never estimate, never carry a number over from a blog.
- Raw output goes in `results/` as committed text files, named
  `YYYY-MM-DD_<condition>.txt` (e.g. `2026-09-07_unpinned.txt`).
- If a result is surprising, that is a finding to explain, not to hide.
- If an experiment fails or is inconclusive, the README says so.

See `docs/MEASUREMENT.md` for methodology.

## 8. Git

- One commit per meaningful step; the project's completing commit is
  `NN: <project name> — <headline result>`.
- Results are committed alongside the code that produced them.

## 9. Definition of done

A project is done when the README's 11 sections are filled, the results directory
has raw output, and you can answer the project's questions out loud without notes.
Compiling is not done.
