#!/usr/bin/env bash
set -euo pipefail

LIMINE_VERSION="10.5.0"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT_DIR="${ROOT_DIR}/limine"
BASE_URL="https://github.com/limine-bootloader/limine/releases/download/v${LIMINE_VERSION}"

mkdir -p "${OUT_DIR}"

TMP_DIR="$(mktemp -d)"
TARBALL="limine-${LIMINE_VERSION}.tar.xz"

wget -O "${TMP_DIR}/${TARBALL}" "${BASE_URL}/${TARBALL}"
tar -xf "${TMP_DIR}/${TARBALL}" -C "${TMP_DIR}"

LIMINE_SRC="${TMP_DIR}/limine-${LIMINE_VERSION}"

(
    cd "${LIMINE_SRC}"
    CC_FOR_TARGET=gcc \
    LD_FOR_TARGET=ld \
    OBJCOPY_FOR_TARGET=objcopy \
    OBJDUMP_FOR_TARGET=objdump \
    READELF_FOR_TARGET=readelf \
    ./configure --enable-uefi-x86-64 --enable-uefi-cd
    make
)

cp "${LIMINE_SRC}/bin/limine-uefi-cd.bin" "${OUT_DIR}/limine-uefi-cd.bin"
cp "${LIMINE_SRC}/bin/BOOTX64.EFI" "${OUT_DIR}/limine-uefi.bin"
