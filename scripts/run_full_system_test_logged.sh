#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOG_PATH="${LOG_PATH:-/tmp/rse_full_test.log}"
NET_LOG_DIR="${NET_LOG_DIR:-/tmp/rse_net_exchange}"
TIMEOUT_BOOT="${TIMEOUT_BOOT:-240}"
TIMEOUT_EXCHANGE="${TIMEOUT_EXCHANGE:-60}"
RSE_BENCH_SMOKE="${RSE_BENCH_SMOKE:-1}"
LIVE_TAIL="${LIVE_TAIL:-0}"
HEARTBEAT_SECS="${HEARTBEAT_SECS:-15}"

mkdir -p "$(dirname "${LOG_PATH}")" "${NET_LOG_DIR}"

echo "==> Full system test (log: ${LOG_PATH})"
export TIMEOUT_BOOT TIMEOUT_EXCHANGE RSE_BENCH_SMOKE
set +e
"${ROOT_DIR}/scripts/run_full_system_test.sh" > "${LOG_PATH}" 2>&1 &
test_pid=$!
set -e

tail_pid=""
if [[ "${LIVE_TAIL}" == "1" ]]; then
  tail -n 20 -f "${LOG_PATH}" &
  tail_pid=$!
fi

while kill -0 "${test_pid}" 2>/dev/null; do
  sleep "${HEARTBEAT_SECS}"
  if ! kill -0 "${test_pid}" 2>/dev/null; then
    break
  fi
  last_line="$(tail -n 1 "${LOG_PATH}" 2>/dev/null || true)"
  if [[ -n "${last_line}" ]]; then
    echo "==> still running; last log: ${last_line}"
  else
    echo "==> still running; log empty yet"
  fi
done

set +e
wait "${test_pid}"
rc=$?
set -e
if [[ -n "${tail_pid}" ]]; then
  kill "${tail_pid}" 2>/dev/null || true
  wait "${tail_pid}" 2>/dev/null || true
fi
if [[ "${rc}" -ne 0 ]]; then
  echo "error: full system test failed; tailing log"
  tail -n 200 "${LOG_PATH}"
  exit "${rc}"
fi

echo "==> Full system test OK; tailing log"
tail -n 200 "${LOG_PATH}"
