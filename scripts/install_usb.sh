#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ISO_PATH="${ROOT_DIR}/build/boot/rse_efi.iso"

if [[ $# -ne 1 ]]; then
    printf 'Usage: %s /dev/sdX\n' "$0" >&2
    exit 2
fi

TARGET="$1"

if [[ ! -e "${TARGET}" ]]; then
    printf 'Target device not found: %s\n' "${TARGET}" >&2
    exit 2
fi

if [[ "$(lsblk -no TYPE "${TARGET}" 2>/dev/null || true)" != "disk" ]]; then
    printf 'Refusing to write to non-disk device: %s\n' "${TARGET}" >&2
    printf 'Tip: pass the disk (e.g. /dev/sdX), not a partition (/dev/sdX1).\n' >&2
    exit 2
fi

if [[ ! -f "${ISO_PATH}" ]]; then
    printf '[RSE] ISO not found, building...\n'
    make -f "${ROOT_DIR}/boot/Makefile.uefi" "${ISO_PATH}"
fi

printf 'About to erase and write %s to %s\n' "${ISO_PATH}" "${TARGET}"
printf 'Type YES to continue: '
read -r CONFIRM
if [[ "${CONFIRM}" != "YES" ]]; then
    printf 'Aborted.\n'
    exit 1
fi

sudo dd if="${ISO_PATH}" of="${TARGET}" bs=4M status=progress oflag=sync
sync
printf '[RSE] USB install complete: %s\n' "${TARGET}"
