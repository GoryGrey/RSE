#!/usr/bin/env python3
import json
import os
import platform
import socket
import threading
import time
from http.server import BaseHTTPRequestHandler, HTTPServer

RESULT_PATH = os.path.join(os.path.dirname(__file__), "linux_baseline.json")

class SimpleHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        payload = b"OK"
        self.send_response(200)
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def log_message(self, fmt, *args):
        return

def compute_test(iters=5_000_000):
    seed = 0x123456789ABCDEF0
    acc = 0
    start = time.perf_counter_ns()
    for i in range(iters):
        seed ^= (seed << 13) & 0xFFFFFFFFFFFFFFFF
        seed ^= (seed >> 7)
        seed ^= (seed << 17) & 0xFFFFFFFFFFFFFFFF
        acc ^= seed + i
    end = time.perf_counter_ns()
    return {
        "ops": iters,
        "ns": end - start,
        "checksum": acc & 0xFFFFFFFFFFFFFFFF,
    }

def memory_test(size_bytes=64 * 1024 * 1024, passes=4):
    a = bytearray(size_bytes)
    b = bytearray(size_bytes)
    for i in range(0, size_bytes, 4096):
        a[i] = (i // 4096) & 0xFF
    start = time.perf_counter_ns()
    checksum = 0
    for p in range(passes):
        for i in range(size_bytes):
            b[i] = (a[i] + p) & 0xFF
            checksum += b[i]
        a, b = b, a
    end = time.perf_counter_ns()
    return {
        "bytes": size_bytes * passes,
        "ns": end - start,
        "checksum": checksum & 0xFFFFFFFFFFFFFFFF,
    }

def file_io_test(root, file_count=128, file_size=4096):
    os.makedirs(root, exist_ok=True)
    payload = bytes((i ^ 0x5A) & 0xFF for i in range(file_size))
    start = time.perf_counter_ns()
    ops = 0
    bytes_moved = 0
    for i in range(file_count):
        path = os.path.join(root, f"file{i:04d}")
        with open(path, "wb") as f:
            f.write(payload)
        ops += 1
        bytes_moved += file_size
        with open(path, "rb") as f:
            data = f.read()
        ops += 1
        bytes_moved += len(data)
        os.stat(path)
        ops += 1
    for i in range(file_count):
        path = os.path.join(root, f"file{i:04d}")
        os.unlink(path)
        ops += 1
    end = time.perf_counter_ns()
    return {
        "ops": ops,
        "bytes": bytes_moved,
        "ns": end - start,
    }

def http_loopback_test(requests=2000):
    try:
        server = HTTPServer(("127.0.0.1", 0), SimpleHandler)
    except PermissionError as exc:
        return {
            "error": f"permission denied: {exc}",
        }
    port = server.server_address[1]
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    start = time.perf_counter_ns()
    total = 0
    for _ in range(requests):
        sock = socket.create_connection(("127.0.0.1", port))
        sock.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n")
        data = sock.recv(1024)
        total += len(data)
        sock.close()
    end = time.perf_counter_ns()
    server.shutdown()
    return {
        "requests": requests,
        "bytes": total,
        "ns": end - start,
    }

def get_os_release():
    info = {}
    try:
        with open("/etc/os-release", "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line or "=" not in line:
                    continue
                key, value = line.split("=", 1)
                info[key] = value.strip().strip('"')
    except FileNotFoundError:
        pass
    return info


def main():
    root = os.path.join(os.path.dirname(__file__), "linux_io_tmp")
    result = {
        "timestamp": time.time(),
        "platform": platform.platform(),
        "os_release": get_os_release(),
        "cpu_count": os.cpu_count(),
        "compute": compute_test(),
        "memory": memory_test(),
        "file_io": file_io_test(root),
        "http_loopback": http_loopback_test(),
    }
    with open(RESULT_PATH, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2)
    print(json.dumps(result, indent=2))

if __name__ == "__main__":
    main()
