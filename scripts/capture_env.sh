#!/usr/bin/env bash
# Dump the machine facts that docs/ENVIRONMENT.md needs.
# Usage: scripts/capture_env.sh > docs/reference/env_capture_$(date +%F).txt
set -uo pipefail

echo "=== date ==="        ; date -Is
echo "=== uname ==="       ; uname -a
echo "=== distro ==="      ; cat /etc/os-release 2>/dev/null | head -3
echo "=== nproc ==="       ; nproc
echo "=== lscpu ==="       ; lscpu
echo "=== caches ==="      ; lscpu -C 2>/dev/null || echo "lscpu -C unavailable"
echo "=== numa ==="        ; numactl --hardware 2>/dev/null || echo "numactl not installed"
echo "=== memory ==="      ; free -h
echo "=== compiler ==="    ; g++ --version | head -1
echo "=== cmake/ninja ===" ; cmake --version | head -1; ninja --version 2>/dev/null
echo "=== governor ==="    ; cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || echo "cpufreq unavailable (expected on WSL2)"
echo "=== hugepages ==="   ; grep -i huge /proc/meminfo
echo "=== cmdline ==="     ; cat /proc/cmdline
