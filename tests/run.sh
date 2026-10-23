#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "$repo_dir/build/host-tests"
"${CXX:-clang++}" -std=c++17 -fsanitize=address,undefined -g \
  -I"$repo_dir/tests/host" "$repo_dir/tests/host/test.cpp" \
  -o "$repo_dir/build/host-tests/test"
for scenario in watchdog watchdog-init-failure watchdog-start-failure \
  offline-recovery offline-rollover telemetry animations-and-storage 60-days-idle; do
  "$repo_dir/build/host-tests/test" "$scenario"
done
