#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOG_DIR="${LOG_DIR:-${ROOT_DIR}/benchmarks/net_exchange}"
BUILD_BASE="${BUILD_BASE:-${ROOT_DIR}/build/net_exchange}"
SHM_PATH="${SHM_PATH:-${BUILD_BASE}/ivshmem.bin}"

cd "${ROOT_DIR}"

NET_MODE="${NET_MODE:-none}"
TORUS_IDS="${TORUS_IDS:-}"
MCAST_ADDR="${MCAST_ADDR:-230.0.0.1:1234}"
TIMEOUT_SECS="${TIMEOUT_SECS:-45}"
NETDEV_DEVICE_OPTS="${NETDEV_DEVICE_OPTS:-disable-modern=on}"
SHM_SIZE="${SHM_SIZE:-65536}"

log_has() {
  local needle="$1"
  local file="$2"
  if command -v rg >/dev/null 2>&1; then
    rg -q --fixed-strings "$needle" "$file"
  else
    grep -qF "$needle" "$file"
  fi
}

if [[ -z "${TORUS_IDS}" ]]; then
  if [[ "${NET_MODE}" == "mcast" ]]; then
    TORUS_IDS="0 1 2"
  else
    TORUS_IDS="0 1 2"
  fi
fi

mkdir -p "${LOG_DIR}"
mkdir -p "${BUILD_BASE}"
if [[ ! -f "${SHM_PATH}" ]]; then
  truncate -s "${SHM_SIZE}" "${SHM_PATH}"
fi

pids=()
idx=0
count=0
for _ in ${TORUS_IDS}; do
  count=$((count + 1))
done
for torus_id in ${TORUS_IDS}; do
  netdev_opts=""
  mac_tail=$(printf "%02x" "${torus_id}")
  if [[ "${NET_MODE}" == "none" ]]; then
    netdev_opts=""
  elif [[ "${NET_MODE}" == "mcast" ]]; then
    netdev_opts="-netdev socket,id=net0,mcast=${MCAST_ADDR} -device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:${mac_tail},${NETDEV_DEVICE_OPTS}"
  elif [[ "${NET_MODE}" == "udp" ]]; then
    if [[ ${count} -ne 2 ]]; then
      echo "NET_MODE=udp requires exactly two TORUS_IDS (got ${count})."
      exit 1
    fi
    if [[ ${idx} -eq 0 ]]; then
      netdev_opts="-netdev socket,id=net0,udp=127.0.0.1:1235,localaddr=127.0.0.1:1234 -device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:${mac_tail},${NETDEV_DEVICE_OPTS}"
    else
      netdev_opts="-netdev socket,id=net0,udp=127.0.0.1:1234,localaddr=127.0.0.1:1235 -device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:${mac_tail},${NETDEV_DEVICE_OPTS}"
    fi
  else
    if [[ ${count} -ne 2 ]]; then
      echo "NET_MODE=pair requires exactly two TORUS_IDS (got ${count})."
      exit 1
    fi
    if [[ ${idx} -eq 0 ]]; then
      netdev_opts="-netdev socket,id=net0,listen=127.0.0.1:1234 -device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:${mac_tail},${NETDEV_DEVICE_OPTS}"
    else
      netdev_opts="-netdev socket,id=net0,connect=127.0.0.1:1234 -device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:${mac_tail},${NETDEV_DEVICE_OPTS}"
    fi
  fi

  build_dir="${BUILD_BASE}/torus${torus_id}"
  log_file="${LOG_DIR}/torus${torus_id}.log"
  serial_log="${LOG_DIR}/torus${torus_id}.serial.log"
  (
    {
      echo "[RSE] run start torus=${torus_id} ts=$(date -u +%s)"
      echo "[RSE] run cfg net_mode=${NET_MODE} timeout=${TIMEOUT_SECS} shm_size=${SHM_SIZE}"
      echo "[RSE] run build_dir=${build_dir}"
      echo "[RSE] run shm_path=${SHM_PATH}"
    } >"${log_file}"
    rm -f "${serial_log}"
    set +e
    timeout --kill-after=5s "${TIMEOUT_SECS}s" \
      make -B -f "${ROOT_DIR}/boot/Makefile.uefi" \
        BUILD_DIR="${build_dir}" \
        RSE_TORUS_ID="${torus_id}" \
        RSE_NET_EXCHANGE=0 \
        RSE_SHM_EXCHANGE=1 \
        NETDEV_OPTS="${netdev_opts}" \
        QEMU_EXTRA_OPTS="-object memory-backend-file,id=ivshmem,share=on,mem-path=${SHM_PATH},size=${SHM_SIZE} -device ivshmem-plain,memdev=ivshmem" \
        QEMU_SERIAL_LOG="${serial_log}" \
        run-iso >>"${log_file}" 2>&1
    rc=$?
    set -e
    echo "[RSE] run exit rc=${rc}" >>"${log_file}"
    if [[ "${rc}" -eq 124 ]]; then
      echo "[RSE] run timeout after ${TIMEOUT_SECS}s" >>"${log_file}"
    fi
    if [[ -f "${serial_log}" ]]; then
      cat "${serial_log}" >>"${log_file}"
    fi
  ) &
  pids+=("$!")
  idx=$((idx + 1))
done

for pid in "${pids[@]}"; do
  wait "${pid}"
done

missing=0
for torus_id in ${TORUS_IDS}; do
  log_file="${LOG_DIR}/torus${torus_id}.log"
  if [[ ! -f "${log_file}" ]]; then
    echo "WARN: torus${torus_id} log missing (${log_file})"
    missing=1
    continue
  fi
  if ! log_has "[RSE] shm projection acked" "${log_file}"; then
    echo "WARN: torus${torus_id} missing projection ack"
    tail -n 20 "${log_file}" || true
    missing=1
  fi
done

if [[ "${missing}" -eq 0 ]]; then
  echo "Projection exchange logs look healthy."
fi

echo "Logs written to ${LOG_DIR}"
