#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOG_DIR="${ROOT_DIR}/benchmarks/net_exchange"
BUILD_BASE="${ROOT_DIR}/build/net_exchange"
SHM_PATH="${SHM_PATH:-${BUILD_BASE}/ivshmem.bin}"

NET_MODE="${NET_MODE:-none}"
TORUS_IDS="${TORUS_IDS:-}"
MCAST_ADDR="${MCAST_ADDR:-230.0.0.1:1234}"
TIMEOUT_SECS="${TIMEOUT_SECS:-45}"
NETDEV_DEVICE_OPTS="${NETDEV_DEVICE_OPTS:-disable-modern=on}"
SHM_SIZE="${SHM_SIZE:-65536}"

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
  (
    timeout "${TIMEOUT_SECS}s" \
      make -B -f "${ROOT_DIR}/boot/Makefile.uefi" \
        BUILD_DIR="${build_dir}" \
        RSE_TORUS_ID="${torus_id}" \
        RSE_NET_EXCHANGE=0 \
        RSE_SHM_EXCHANGE=1 \
        NETDEV_OPTS="${netdev_opts}" \
        QEMU_EXTRA_OPTS="-object memory-backend-file,id=ivshmem,share=on,mem-path=${SHM_PATH},size=${SHM_SIZE} -device ivshmem-plain,memdev=ivshmem" \
        run-iso >"${log_file}" 2>&1 || true
  ) &
  pids+=("$!")
  idx=$((idx + 1))
done

for pid in "${pids[@]}"; do
  wait "${pid}"
done

echo "Logs written to ${LOG_DIR}"
