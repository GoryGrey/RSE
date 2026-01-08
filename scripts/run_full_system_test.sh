#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BOOT_LOG_DIR="${BOOT_LOG_DIR:-${ROOT_DIR}/build/boot}"
BOOT_LOG="${BOOT_LOG:-${BOOT_LOG_DIR}/boot.log}"
BOOT_LOG_RAW="${BOOT_LOG_RAW:-${BOOT_LOG_DIR}/boot_raw.log}"
BOOT_MAKE_LOG="${BOOT_MAKE_LOG:-${BOOT_LOG_DIR}/boot.make.log}"
BOOT_RAW_MAKE_LOG="${BOOT_RAW_MAKE_LOG:-${BOOT_LOG_DIR}/boot_raw.make.log}"
NET_LOG_DIR="${NET_LOG_DIR:-${ROOT_DIR}/benchmarks/net_exchange}"
TIMEOUT_BOOT="${TIMEOUT_BOOT:-240}"
TIMEOUT_EXCHANGE="${TIMEOUT_EXCHANGE:-45}"
RSE_BENCH_SMOKE="${RSE_BENCH_SMOKE:-1}"
RSE_NET_RAW_TEST="${RSE_NET_RAW_TEST:-0}"

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

validate_exchange_logs() {
  for torus_id in 0 1 2; do
    local log="${NET_LOG_DIR}/torus${torus_id}.log"
    [[ -f "${log}" ]] || return 1
    rg -q --fixed-strings "[RSE] shm projection online" "${log}" || return 1
    rg -q --fixed-strings "[RSE] shm projection exchange start" "${log}" || return 1
    rg -q --fixed-strings "[RSE] shm projection acked seq=1" "${log}" || return 1
    rg -q --fixed-strings "[RSE] shm projection recv torus=" "${log}" || return 1
  done
  return 0
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
  exec_vfs_test
  blockfs_test
  net_device_test
  sys_wait_test
  sys_kill_test
  sys_ps_test
  sys_stat_test
  sys_memfs_dir_test
  sys_user_isolation_test
  sys_vfs_persist_test
  sys_socket_test
  sys_socket_net_test
  sys_socket_tcp_test
  sys_pipe_test
  sys_dup_test
  sys_mmap_test
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
export RSE_BENCH_SMOKE
rm -f "${BOOT_LOG}"
set +e
QEMU_SERIAL_LOG="${BOOT_LOG}" \
  timeout "${TIMEOUT_BOOT}s" make -f "${ROOT_DIR}/boot/Makefile.uefi" run-iso >"${BOOT_MAKE_LOG}" 2>&1
boot_rc=$?
set -e
if [[ "${boot_rc}" -ne 0 && "${boot_rc}" -ne 124 ]]; then
  fail "UEFI boot failed (exit ${boot_rc})"
fi
check_log "${BOOT_LOG}" "[RSE] UEFI kernel start"
check_log "${BOOT_LOG}" "[RSE] benchmarks end"
check_log "${BOOT_LOG}" "[RSE] UEFI keyboard online"

if [[ "${RSE_NET_RAW_TEST}" == "1" ]]; then
  say "Boot UEFI ISO (raw TCP) and capture log"
  rm -f "${BOOT_LOG_RAW}"
  set +e
  QEMU_SERIAL_LOG="${BOOT_LOG_RAW}" \
    timeout "${TIMEOUT_BOOT}s" make -f "${ROOT_DIR}/boot/Makefile.uefi" RSE_NET_RAW=1 run-iso >"${BOOT_RAW_MAKE_LOG}" 2>&1
  boot_raw_rc=$?
  set -e
  if [[ "${boot_raw_rc}" -ne 0 && "${boot_raw_rc}" -ne 124 ]]; then
    fail "UEFI raw TCP boot failed (exit ${boot_raw_rc})"
  fi
  check_log "${BOOT_LOG_RAW}" "[RSE] UEFI kernel start"
  check_log "${BOOT_LOG_RAW}" "[init] rawtcp ok"
  check_log "${BOOT_LOG_RAW}" "[RSE] benchmarks end"
fi

say "Run projection exchange (SHM, 3 VMs)"
exchange_timeout="${TIMEOUT_EXCHANGE}"
exchange_ok=0
for attempt in 1 2 3; do
  say "Projection exchange attempt ${attempt}/3 (timeout ${exchange_timeout}s, logs=${NET_LOG_DIR})"
  rm -f "${NET_LOG_DIR}/torus0.log" "${NET_LOG_DIR}/torus1.log" "${NET_LOG_DIR}/torus2.log"
  TIMEOUT_SECS="${exchange_timeout}" LOG_DIR="${NET_LOG_DIR}" "${ROOT_DIR}/scripts/run_projection_exchange.sh"
  if validate_exchange_logs; then
    exchange_ok=1
    break
  fi
  exchange_timeout=$((exchange_timeout + 30))
done
if [[ "${exchange_ok}" -ne 1 ]]; then
  fail "projection exchange logs missing expected markers after retries"
fi

say "Full system test OK"
