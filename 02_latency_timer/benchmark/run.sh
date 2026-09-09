#!/usr/bin/env bash
# Capture harness for Project 02.
#
#   benchmark/run.sh <experiment-binary> <reps> <label> [args...]
#
# Writes results/<today>_<label>.txt with a header recording the exact command,
# machine and build, so every number in README.md traces back to a rerunnable
# command. Example:
#
#   benchmark/run.sh 02_exp1_clock_resolution   5 clock_resolution
#   benchmark/run.sh 02_exp3_percentile_capture 5 percentile_capture
#
set -euo pipefail

if [[ $# -lt 3 ]]; then
  echo "usage: $0 <experiment-binary> <reps> <label> [args...]" >&2
  exit 1
fi

bin_name=$1; reps=$2; label=$3; shift 3

root=$(cd "$(dirname "$0")/../.." && pwd)
bin="$root/build/02_latency_timer/$bin_name"
out="$(cd "$(dirname "$0")/.." && pwd)/results/$(date +%F)_${label}.txt"

if [[ ! -x $bin ]]; then
  echo "error: $bin not built. Run: cmake --build build" >&2
  exit 1
fi

{
  echo "# Command: build/02_latency_timer/$bin_name $*   (repeated ${reps}x)"
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
