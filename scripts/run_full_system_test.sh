#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BOOT_LOG_DIR="${ROOT_DIR}/build/boot"
BOOT_LOG="${BOOT_LOG_DIR}/boot.log"
NET_LOG_DIR="${ROOT_DIR}/benchmarks/net_exchange"
TIMEOUT_BOOT="${TIMEOUT_BOOT:-90}"
TIMEOUT_EXCHANGE="${TIMEOUT_EXCHANGE:-45}"

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

for cmd in cmake make timeout qemu-system-x86_64 xorriso mkfs.fat mcopy mmd sgdisk rg truncate; do
  require_cmd "$cmd"
done

say "Build native C++ kernel"
cmake -S "${ROOT_DIR}/src/cpp_kernel" -B "${ROOT_DIR}/build/cpp_kernel"
cmake --build "${ROOT_DIR}/build/cpp_kernel"

say "Run native kernel tests"
CPP_TESTS=(
  allocator_test
  fixed_structures_test
  threadsafe_scheduler_test
  memory_telemetry_test
  killer_demo_memory_test
  c_api_test
  elf_loader_test
  elf_process_test
)
for test_bin in "${CPP_TESTS[@]}"; do
  test_path="${ROOT_DIR}/build/cpp_kernel/${test_bin}"
  [[ -x "${test_path}" ]] || fail "missing test binary: ${test_path}"
  "${test_path}"
done

say "Build braided demo/tests"
cmake -S "${ROOT_DIR}/src/cpp_kernel/braided" -B "${ROOT_DIR}/build/braided"
cmake --build "${ROOT_DIR}/build/braided"

say "Run braided demo/tests"
"${ROOT_DIR}/build/braided/braided_demo"
"${ROOT_DIR}/build/braided/test_braided"

say "Build UEFI ISO"
make -f "${ROOT_DIR}/boot/Makefile.uefi" "${ROOT_DIR}/build/boot/rse_efi.iso"

say "Boot UEFI ISO (headless) and capture log"
mkdir -p "${BOOT_LOG_DIR}"
set +e
timeout "${TIMEOUT_BOOT}s" make -f "${ROOT_DIR}/boot/Makefile.uefi" run-iso 2>&1 | tee "${BOOT_LOG}"
boot_rc=${PIPESTATUS[0]}
set -e
if [[ "${boot_rc}" -ne 0 && "${boot_rc}" -ne 124 ]]; then
  fail "UEFI boot failed (exit ${boot_rc})"
fi
check_log "${BOOT_LOG}" "[RSE] UEFI kernel start"
check_log "${BOOT_LOG}" "[RSE] benchmarks end"
check_log "${BOOT_LOG}" "[RSE] UEFI keyboard online"

say "Run projection exchange (SHM, 3 VMs)"
TIMEOUT_SECS="${TIMEOUT_EXCHANGE}" "${ROOT_DIR}/scripts/run_projection_exchange.sh"
for torus_id in 0 1 2; do
  log="${NET_LOG_DIR}/torus${torus_id}.log"
  [[ -f "${log}" ]] || fail "missing exchange log: ${log}"
  check_log "${log}" "[RSE] shm projection online"
  check_log "${log}" "[RSE] shm projection exchange start"
  check_log "${log}" "[RSE] shm projection acked seq=1"
  check_log "${log}" "[RSE] shm projection recv torus="
done

say "Full system test OK"
