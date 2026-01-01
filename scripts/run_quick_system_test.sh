#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BOOT_LOG_DIR="${BOOT_LOG_DIR:-${ROOT_DIR}/build/boot}"
BOOT_LOG="${BOOT_LOG:-${BOOT_LOG_DIR}/quick_test.log}"
TIMEOUT_BOOT="${TIMEOUT_BOOT:-120}"
RSE_BENCH_SMOKE="${RSE_BENCH_SMOKE:-1}"

say() {
  echo "==> $*"
}

fail() {
  echo "error: $*" >&2
  exit 1
}

require_cmd() {
  command -v "$1" >/dev/null 2>&1 || fail "missing required command: $1"
}

check_log() {
  local file="$1"
  local pattern="$2"
  rg -q --fixed-strings "$pattern" "$file" || fail "missing '$pattern' in $file"
}

for cmd in make timeout qemu-system-x86_64 xorriso mkfs.fat mcopy mmd sgdisk rg truncate; do
  require_cmd "$cmd"
done

mkdir -p "${BOOT_LOG_DIR}"

say "Boot UEFI ISO (quick, headless) and capture log"
export RSE_BENCH_SMOKE
set +e
timeout "${TIMEOUT_BOOT}s" make -B -f "${ROOT_DIR}/boot/Makefile.uefi" run-iso 2>&1 | tee "${BOOT_LOG}"
boot_rc=${PIPESTATUS[0]}
set -e
if [[ "${boot_rc}" -ne 0 && "${boot_rc}" -ne 124 ]]; then
  fail "UEFI boot failed (exit ${boot_rc})"
fi

check_log "${BOOT_LOG}" "[RSE] UEFI kernel start"
check_log "${BOOT_LOG}" "[RSE] benchmarks end"
check_log "${BOOT_LOG}" "[RSE] UEFI keyboard online"

say "Quick system test OK"
