#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

make -f "${ROOT_DIR}/boot/Makefile.uefi" RSE_AUTO_EXIT=1 run-iso
