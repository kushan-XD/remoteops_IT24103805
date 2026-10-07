import socket
import uuid
from pathlib import Path

ADDRESS = ("127.0.0.1", 9410)
STORAGE = Path("agentfiles/IT24103805")
PREFIX = "put-test-" + uuid.uuid4().hex[:12]


def connect(authenticate=True):
    sock = socket.create_connection(ADDRESS, timeout=5)
    reader = sock.makefile("rb")
    if authenticate:
        sock.sendall(b"AUTH OPS-3805\n")
        assert reader.readline() == b"OK AUTHENTICATED SID:5083\n"
    return sock, reader


def upload(name, payload):
    sock, reader = connect()
    try:
        header = f"PUT {name} {len(payload)}\n".encode()

        # Send header, binary body and next command together.
        sock.sendall(header + payload + b"SYSINFO\n")
        expected = f"OK FILE_RECEIVED {name} SID:5083\n".encode()
        reply = reader.readline()
        assert reply == expected, reply

        reply = reader.readline()
        assert reply.startswith(b"OK SYSINFO "), reply
        assert reply.endswith(b" SID:5083\n"), reply
        assert (STORAGE / name).read_bytes() == payload

        sock.sendall(b"QUIT\n")
        assert reader.readline() == b"OK BYE SID:5083\n"
        assert reader.read(1) == b""
    finally:
        reader.close()
        sock.close()


# Includes every byte value, NULs and newlines; exceeds reader buffer.
upload(PREFIX + ".bin", bytes(range(256)) * 80)
print("PASS: exact binary upload and following SYSINFO")

upload(PREFIX + "-empty.bin", b"")
print("PASS: empty upload")

# Preserve an existing complete file when its replacement is incomplete.
name = PREFIX + "-preserved.bin"
upload(name, b"original complete file")
temporary_before = set(STORAGE.glob(".upload-*"))

sock, reader = connect()
try:
    sock.sendall(f"PUT {name} 100\n".encode() + b"partial")
    sock.shutdown(socket.SHUT_WR)
    reply = reader.readline()
    assert reply == b"ERR 008 IO_ERROR SID:5083\n", reply
    assert reader.read(1) == b""
finally:
    reader.close()
    sock.close()

assert (STORAGE / name).read_bytes() == b"original complete file"
assert set(STORAGE.glob(".upload-*")) == temporary_before
print("PASS: incomplete upload removed; previous file preserved")

# Oversized uploads must be rejected without waiting for their body.
sock, reader = connect()
try:
    sock.sendall(f"PUT {PREFIX}-large.bin 67108865\n".encode())
    assert reader.readline() == b"ERR 004 FILE_TOO_LARGE SID:5083\n"
    assert reader.read(1) == b""
finally:
    reader.close()
    sock.close()
print("PASS: oversized upload rejected and connection closed")

sock, reader = connect()
try:
    sock.sendall(b"PUT ../escape.bin 0\n")
    assert reader.readline() == b"ERR 006 BAD_REQUEST SID:5083\n"
    assert reader.read(1) == b""
finally:
    reader.close()
    sock.close()
print("PASS: path traversal rejected")

sock, reader = connect(authenticate=False)
try:
    sock.sendall(f"PUT {PREFIX}-unauth.bin 0\n".encode())
    assert reader.readline() == b"ERR 003 AUTH_REQUIRED SID:5083\n"
    assert reader.read(1) == b""
finally:
    reader.close()
    sock.close()
print("PASS: unauthenticated upload rejected")

print("All PUT checkpoint tests passed.")
