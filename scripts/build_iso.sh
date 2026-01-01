#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

make -f "${ROOT_DIR}/boot/Makefile.uefi" "${ROOT_DIR}/build/boot/rse_efi.iso"
printf '[RSE] ISO: %s\n' "${ROOT_DIR}/build/boot/rse_efi.iso"
