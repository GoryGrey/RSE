#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOG_DIR="$ROOT_DIR/benchmarks"
LOG_FILE="$LOG_DIR/uefi_serial.log"
MAKE_LOG="$LOG_DIR/uefi_make.log"
JSON_OUT="$LOG_DIR/uefi_bench.json"
CSV_OUT="$LOG_DIR/uefi_bench.csv"

mkdir -p "$LOG_DIR"

# Capture serial output from QEMU run.
set +e
rm -f "$LOG_FILE"
QEMU_SERIAL_LOG="$LOG_FILE" timeout 40s make -B -f "$ROOT_DIR/boot/Makefile.uefi" RSE_AUTO_EXIT=1 run-iso >"$MAKE_LOG" 2>&1
set -e

python3 "$ROOT_DIR/scripts/parse_uefi_bench.py" \
  --log "$LOG_FILE" \
  --json "$JSON_OUT" \
  --csv "$CSV_OUT" \
  --status "$ROOT_DIR/PROJECT_STATUS.md"
