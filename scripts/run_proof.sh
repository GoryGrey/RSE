#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOG="$ROOT/build/boot/proof.log"

NETDEV_HOSTFWD=",hostfwd=udp::8080-:8080,hostfwd=udp::40001-:40000"
QEMU_EXTRA_OPTS=${QEMU_EXTRA_OPTS:-"-display none"}

mkdir -p "$ROOT/build/boot"
make -f "$ROOT/boot/Makefile.uefi"

( NETDEV_HOSTFWD="$NETDEV_HOSTFWD" QEMU_EXTRA_OPTS="$QEMU_EXTRA_OPTS" \
  make -f "$ROOT/boot/Makefile.uefi" run > "$LOG" 2>&1 ) &
PID=$!

sleep 4
"$ROOT/scripts/send_udp_http.py" --mode http --count 200 --port 8080 || true
"$ROOT/scripts/send_udp_http.py" --mode udp --count 200 --port 40001 || true
sleep 6

kill "$PID" >/dev/null 2>&1 || true
wait "$PID" >/dev/null 2>&1 || true

printf "Proof log: %s\n" "$LOG"
grep -E "\[user\]|udp/http server|net arp|virtio-blk|benchmarks end" "$LOG" || true
