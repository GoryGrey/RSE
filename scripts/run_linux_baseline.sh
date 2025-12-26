#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
python3 "${ROOT}/benchmarks/linux_baseline.py" | tee "${ROOT}/benchmarks/linux_baseline.json"
