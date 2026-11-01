#!/bin/sh
# Build and run the core tests (no JUCE needed).
set -e
cd "$(dirname "$0")/.."
OUT="${TMPDIR:-/tmp}/fp_test"
g++ -std=c++17 -O2 -Wall -Wextra -fsanitize=address,undefined -o "$OUT" tests/test_core.cpp
"$OUT"
