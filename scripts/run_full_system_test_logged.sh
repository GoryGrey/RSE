#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOG_PATH="${LOG_PATH:-/tmp/rse_full_test.log}"
NET_LOG_DIR="${NET_LOG_DIR:-/tmp/rse_net_exchange}"
TIMEOUT_BOOT="${TIMEOUT_BOOT:-60}"
TIMEOUT_EXCHANGE="${TIMEOUT_EXCHANGE:-60}"

mkdir -p "$(dirname "${LOG_PATH}")" "${NET_LOG_DIR}"

echo "==> Full system test (log: ${LOG_PATH})"
if ! "${ROOT_DIR}/scripts/run_full_system_test.sh" > "${LOG_PATH}" 2>&1; then
  echo "error: full system test failed; tailing log"
  tail -n 200 "${LOG_PATH}"
  exit 1
fi

echo "==> Full system test OK; tailing log"
tail -n 200 "${LOG_PATH}"
