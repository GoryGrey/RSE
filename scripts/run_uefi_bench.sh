#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOG_DIR="$ROOT_DIR/benchmarks"
LOG_FILE="$LOG_DIR/uefi_serial.log"
JSON_OUT="$LOG_DIR/uefi_bench.json"
CSV_OUT="$LOG_DIR/uefi_bench.csv"

mkdir -p "$LOG_DIR"

# Capture serial output from QEMU run.
set +e
timeout 40s make -B -f "$ROOT_DIR/boot/Makefile.uefi" run-iso 2>&1 | tee "$LOG_FILE"
set -e

python3 "$ROOT_DIR/scripts/parse_uefi_bench.py" \
  --log "$LOG_FILE" \
  --json "$JSON_OUT" \
  --csv "$CSV_OUT" \
  --status "$ROOT_DIR/PROJECT_STATUS.md"

