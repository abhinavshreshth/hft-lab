#!/usr/bin/env bash
# Capture harness for Project 05.
#
#   benchmark/run.sh <experiment-binary> <reps> <label> [args...]
#
# Writes results/<today>_<label>.txt with a header recording the exact command,
# machine and build, so every number in README.md traces back to a rerunnable
# command. Example:
#
#   benchmark/run.sh 05_exp1_layout_sanity   5 sanity
#   benchmark/run.sh 05_exp2_packed          5 packed
#   benchmark/run.sh 05_exp3_padded          5 padded
#   benchmark/run.sh 05_exp4_compare         5 compare
#   benchmark/run.sh 05_exp5_thread_scaling  5 thread_scaling
#   benchmark/run.sh 05_exp6_offset_sweep    5 offset_sweep
#   benchmark/run.sh 05_exp7_placement_sweep 5 placement_sweep
#
set -euo pipefail

if [[ $# -lt 3 ]]; then
  echo "usage: $0 <experiment-binary> <reps> <label> [args...]" >&2
  exit 1
fi

bin_name=$1; reps=$2; label=$3; shift 3

root=$(cd "$(dirname "$0")/../.." && pwd)
bin="$root/build/05_false_sharing/$bin_name"
out="$(cd "$(dirname "$0")/.." && pwd)/results/$(date +%F)_${label}.txt"

if [[ ! -x $bin ]]; then
  echo "error: $bin not built. Run: cmake --build build" >&2
  exit 1
fi

{
  echo "# Command: build/05_false_sharing/$bin_name $*   (repeated ${reps}x)"
  echo "# Date:    $(date -Is)"
  echo "# Machine: $(uname -sr) — $(nproc) logical CPUs (see docs/ENVIRONMENT.md)"
  echo "# Build:   Release (-O2 -g), $(g++ --version | head -1)"
  echo
  for ((i = 1; i <= reps; i++)); do
    echo "--- repetition $i ---"
    "$bin" "$@"
    echo
  done
} > "$out"

echo "wrote $out"
