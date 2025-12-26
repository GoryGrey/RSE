#!/usr/bin/env python3
import argparse
import json
import re
from datetime import datetime, timezone
from pathlib import Path

PATTERNS = {
    "init_compute": re.compile(r"^\[init\] compute ops=(\d+) cycles=(\d+) checksum=(\d+)") ,
    "init_memfs": re.compile(r"^\[init\] memfs ops=(\d+) bytes=(\d+) cycles=(\d+)") ,
    "init_blk0": re.compile(r"^\[init\] /dev/blk0 size=(\d+) ops=(\d+) bytes=(\d+) mismatches=(\d+) cycles=(\d+)") ,
    "init_loop": re.compile(r"^\[init\] loopback wrote=(\d+) read=(\d+)") ,
    "init_net": re.compile(r"^\[init\] net0 wrote=(\d+) read=(\d+)") ,
    "k_compute": re.compile(r"^\[RSE\] compute ops=(\d+) cycles=(\d+) cycles/op=(\d+) checksum=(\d+)") ,
    "k_memory": re.compile(r"^\[RSE\] memory bytes=(\d+) cycles=(\d+) cycles/byte=(\d+)") ,
    "k_ramfs": re.compile(r"^\[RSE\] ramfs ops=(\d+) cycles=(\d+) cycles/op=(\d+) checksum=(\d+) files=(\d+)") ,
    "k_uefi_fs": re.compile(r"^\[RSE\] UEFI FS ops=(\d+) cycles=(\d+) cycles/op=(\d+) checksum=(\d+)") ,
    "k_uefi_blk": re.compile(r"^\[RSE\] UEFI block bytes=(\d+) write cycles=(\d+) write cycles/byte=(\d+) read cycles=(\d+) read cycles/byte=(\d+) checksum=(\d+)") ,
    "k_http": re.compile(r"^\[RSE\] http requests=(\d+) cycles=(\d+) cycles/req=(\d+) checksum=(\d+)") ,
}


def parse_log(lines):
    data = {"timestamp": datetime.now(timezone.utc).isoformat()}
    virtio_blk_status = None
    for line in lines:
        line = line.strip()
        if "virtio-blk timeout" in line:
            virtio_blk_status = "timeout"
        for key, regex in PATTERNS.items():
            m = regex.match(line)
            if m:
                data[key] = [int(x) for x in m.groups()]
    if virtio_blk_status:
        data["virtio_blk_status"] = virtio_blk_status
    return data


def write_json(path, data):
    Path(path).write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def write_csv(path, data):
    flat = {"timestamp": data.get("timestamp", "")}
    for key, vals in data.items():
        if key == "timestamp":
            continue
        if isinstance(vals, list):
            for i, val in enumerate(vals):
                flat[f"{key}_{i}"] = val
        else:
            flat[key] = vals
    path = Path(path)
    if not path.exists():
        header = ",".join(flat.keys())
        row = ",".join(str(flat[k]) for k in flat.keys())
        path.write_text(header + "\n" + row + "\n", encoding="utf-8")
        return
    existing = path.read_text(encoding="utf-8").splitlines()
    header = existing[0].split(",")
    row = ",".join(str(flat.get(k, "")) for k in header)
    path.write_text("\n".join(existing + [row]) + "\n", encoding="utf-8")


def update_status(path, data):
    status_path = Path(path)
    text = status_path.read_text(encoding="utf-8")

    def repl(pattern, new_line):
        nonlocal text
        if re.search(pattern, text, flags=re.M):
            text = re.sub(pattern, new_line, text, flags=re.M)

    if "k_compute" in data:
        ops, cycles, per_op, _ = data["k_compute"]
        repl(r"^- \*\*Compute\*\*: .*$",
             f"- **Compute**: {ops:,} ops, {cycles:,} cycles ({per_op} cycles/op)")
    if "k_memory" in data:
        bytes_, cycles, per_byte = data["k_memory"]
        repl(r"^- \*\*Memory\*\*: .*$",
             f"- **Memory**: {bytes_:,} bytes, {cycles:,} cycles ({per_byte} cycles/byte)")
    if "k_ramfs" in data:
        ops, cycles, per_op, _, _ = data["k_ramfs"]
        repl(r"^- \*\*RAMFS File I/O\*\*: .*$",
             f"- **RAMFS File I/O**: {ops} ops, {cycles:,} cycles ({per_op} cycles/op)")
    if "k_uefi_fs" in data:
        ops, cycles, per_op, _ = data["k_uefi_fs"]
        repl(r"^- \*\*UEFI FAT File I/O.*$",
             f"- **UEFI FAT File I/O (USB disk)**: {ops} ops, {cycles:,} cycles ({per_op} cycles/op)")
    if "k_uefi_blk" in data:
        bytes_, w_cycles, w_per, r_cycles, r_per, _ = data["k_uefi_blk"]
        repl(r"^- \*\*UEFI Raw Block I/O.*$",
             f"- **UEFI Raw Block I/O (USB disk)**: {bytes_} bytes, write {w_cycles:,} cycles ({w_per} cycles/byte), read {r_cycles:,} cycles ({r_per} cycles/byte)")
    if "k_http" in data:
        reqs, cycles, per_req, _ = data["k_http"]
        repl(r"^- \*\*HTTP Loopback\*\*: .*$",
             f"- **HTTP Loopback**: {reqs} requests, {cycles:,} cycles ({per_req} cycles/req)")

    # Update device smoke tests line.
    if "init_blk0" in data or "init_loop" in data or "init_net" in data:
        blk = data.get("init_blk0")
        loop = data.get("init_loop")
        net = data.get("init_net")
        parts = []
        if blk:
            size, ops, bytes_, mismatches, _ = blk
            parts.append(f"/dev/blk0 ({size}B, {ops} ops, {bytes_} bytes, {mismatches} mismatches)")
        if loop:
            wrote, read = loop
            parts.append(f"/dev/loopback ({wrote}B echo, {read}B read)")
        if net:
            wrote, read = net
            parts.append(f"/dev/net0 ({wrote}B tx, {read}B rx)")
        new_line = "- **Init Device Smoke Tests**: " + ", ".join(parts)
        if re.search(r"^- \*\*Init Device Smoke Tests\*\*: .*$", text, flags=re.M):
            text = re.sub(r"^- \*\*Init Device Smoke Tests\*\*: .*$", new_line, text, flags=re.M)
        else:
            text = re.sub(r"^- \*\*HTTP Loopback\*\*: .*$", r"\g<0>\n" + new_line, text, flags=re.M)

    status_path.write_text(text, encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--log", required=True)
    parser.add_argument("--json", required=True)
    parser.add_argument("--csv", required=True)
    parser.add_argument("--status", required=True)
    args = parser.parse_args()

    lines = Path(args.log).read_text(encoding="utf-8", errors="ignore").splitlines()
    data = parse_log(lines)

    write_json(args.json, data)
    write_csv(args.csv, data)
    update_status(args.status, data)
