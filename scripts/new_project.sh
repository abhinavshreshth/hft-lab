#!/usr/bin/env bash
# Scaffold one project directory from docs/templates/project_template.
#
#   scripts/new_project.sh 01 cpu_pinned_worker "CPU-Pinned Worker" 1
#
# Creates NN_dirname/ with src/, benchmark/, results/, README.md, CMakeLists.txt.
# Refuses to touch an existing directory.
set -euo pipefail

if [[ $# -lt 3 ]]; then
  echo "usage: $0 <NN> <dir_suffix> <Project Name> [stage]" >&2
  echo "example: $0 01 cpu_pinned_worker \"CPU-Pinned Worker\" 1" >&2
  exit 1
fi

nn=$1; suffix=$2; name=$3; stage=${4:-?}
root=$(cd "$(dirname "$0")/.." && pwd)
tmpl="$root/docs/templates/project_template"
dir="$root/${nn}_${suffix}"
target="${nn}_${suffix}"

[[ $nn =~ ^[0-9]{2}$ ]] || { echo "error: NN must be two digits (got '$nn')" >&2; exit 1; }
[[ -d $dir ]] && { echo "error: $dir already exists — refusing to overwrite" >&2; exit 1; }

cp -r "$tmpl" "$dir"
rm -f "$dir/src/.gitkeep"   # benchmark/.gitkeep stays so the empty dir is tracked

sed -i "s/__TARGET__/${target}/g" "$dir/CMakeLists.txt"
sed -i -e "s/__NN__/${nn}/g" -e "s/__NAME__/${name}/g" -e "s/__STAGE__/${stage}/g" "$dir/README.md"

cat > "$dir/src/main.cpp" <<'CPP'
int main() { return 0; }
CPP

echo "created $dir"
echo "next: write src/, then 'cmake -S . -B build -G Ninja && cmake --build build'"
echo "      and tick the row in docs/PROGRESS.md when the README is filled."
