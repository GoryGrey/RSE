#!/usr/bin/env python3
import argparse
import socket
import time

def main() -> int:
    parser = argparse.ArgumentParser(description="Send UDP payloads to RSE net server")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--count", type=int, default=100)
    parser.add_argument("--delay-ms", type=int, default=5)
    parser.add_argument("--mode", choices=["http", "udp"], default="http")
    args = parser.parse_args()

    if args.mode == "http":
        payload = b"GET / HTTP/1.1\r\nHost: rse\r\n\r\n"
    else:
        payload = b"rse-udp-ping"

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    for _ in range(args.count):
        sock.sendto(payload, (args.host, args.port))
        if args.delay_ms > 0:
            time.sleep(args.delay_ms / 1000.0)
    sock.close()
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
