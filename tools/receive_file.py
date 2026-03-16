#!/usr/bin/env python3
"""
receive_file.py — TCP file receive server for the ESP32 SD card reader.

Run on the Raspberry Pi before triggering a file transfer from the ESP32.

Protocol (ESP32 → Pi):
  [4 bytes big-endian: filename length]
  [N bytes: filename (no null terminator)]
  [8 bytes big-endian: file size in bytes]
  [file data streamed in chunks]

On success the server sends a 1-byte ACK (0x06) and saves the file to
the OUTPUT_DIR directory.

Usage:
  python3 receive_file.py [--port PORT] [--output-dir DIR]
  Default port: 5000
  Default output directory: ~/received_files/
"""

import argparse
import pathlib
import socket
import struct
import sys


DEFAULT_PORT = 5000
DEFAULT_OUTPUT_DIR = pathlib.Path.home() / "received_files"
ACK = bytes([0x06])


def recv_exactly(conn: socket.socket, n: int) -> bytes:
    """Read exactly n bytes from conn, raising if the connection closes early."""
    buf = b""
    while len(buf) < n:
        chunk = conn.recv(n - len(buf))
        if not chunk:
            raise ConnectionError(f"Connection closed after {len(buf)}/{n} bytes")
        buf += chunk
    return buf


def handle_connection(conn: socket.socket, addr: tuple, output_dir: pathlib.Path) -> None:
    print(f"[+] Connection from {addr[0]}:{addr[1]}")

    # Read filename length (4 bytes BE)
    name_len = struct.unpack(">I", recv_exactly(conn, 4))[0]
    if name_len == 0 or name_len > 255:
        print(f"[!] Invalid filename length: {name_len}")
        return

    # Read filename
    filename = recv_exactly(conn, name_len).decode("utf-8", errors="replace")
    print(f"    Filename : {filename}")

    # Read file size (8 bytes BE)
    file_size = struct.unpack(">Q", recv_exactly(conn, 8))[0]
    print(f"    File size: {file_size} bytes")

    # Write file
    output_dir.mkdir(parents=True, exist_ok=True)
    dest = output_dir / pathlib.Path(filename).name  # strip any path components
    received = 0
    with open(dest, "wb") as f:
        while received < file_size:
            chunk = conn.recv(min(4096, file_size - received))
            if not chunk:
                raise ConnectionError(f"Connection closed at {received}/{file_size} bytes")
            f.write(chunk)
            received += len(chunk)

    print(f"    Saved to : {dest}")
    conn.sendall(ACK)
    print(f"    ACK sent")


def main() -> None:
    parser = argparse.ArgumentParser(description="ESP32 TCP file receive server")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--output-dir", type=pathlib.Path, default=DEFAULT_OUTPUT_DIR)
    args = parser.parse_args()

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as srv:
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        srv.bind(("", args.port))
        srv.listen(1)
        print(f"Listening on port {args.port} — saving files to {args.output_dir}")

        while True:
            try:
                conn, addr = srv.accept()
                with conn:
                    handle_connection(conn, addr, args.output_dir)
            except ConnectionError as e:
                print(f"[!] Transfer error: {e}", file=sys.stderr)
            except KeyboardInterrupt:
                print("\nExiting.")
                break


if __name__ == "__main__":
    main()
