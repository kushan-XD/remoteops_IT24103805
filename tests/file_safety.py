import socket
import uuid
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from threading import Barrier

ADDRESS = ("127.0.0.1", 9410)
STORAGE = Path("agentfiles/IT24103805")
PREFIX = "safety-" + uuid.uuid4().hex[:12]


def connect():
    sock = socket.create_connection(ADDRESS, timeout=10)
    reader = sock.makefile("rb")
    sock.sendall(b"AUTH OPS-3805\n")
    assert reader.readline() == b"OK AUTHENTICATED SID:5083\n"
    return sock, reader


# Use only new, uniquely named fixtures.
target = STORAGE / (PREFIX + "-target.bin")
link = STORAGE / (PREFIX + "-link.bin")
target.write_bytes(b"protected original")
link.symlink_to(target.name)

try:
    sock, reader = connect()
    try:
        sock.sendall(f"GET {link.name}\n".encode())
        reply = reader.readline()
        assert reply == b"ERR 005 FILE_NOT_FOUND SID:5083\n", reply

        sock.sendall(f"PUT {link.name} 0\n".encode())
        reply = reader.readline()
        assert reply == b"ERR 008 IO_ERROR SID:5083\n", reply
        assert reader.read(1) == b""
    finally:
        reader.close()
        sock.close()

    assert link.is_symlink()
    assert target.read_bytes() == b"protected original"
    print("PASS: GET and PUT reject symlink; target unchanged")
finally:
    link.unlink(missing_ok=True)
    target.unlink(missing_ok=True)


name = PREFIX + "-concurrent.bin"
payloads = [bytes([value]) * 131072 for value in range(1, 6)]
barrier = Barrier(len(payloads))


def upload(payload):
    sock, reader = connect()
    try:
        barrier.wait(timeout=10)
        sock.sendall(f"PUT {name} {len(payload)}\n".encode() + payload)
        reply = reader.readline()
        assert reply == f"OK FILE_RECEIVED {name} SID:5083\n".encode(), reply
        sock.sendall(b"QUIT\n")
        assert reader.readline() == b"OK BYE SID:5083\n"
    finally:
        reader.close()
        sock.close()


temporary_before = set(STORAGE.glob(".upload-*"))

with ThreadPoolExecutor(max_workers=5) as workers:
    list(workers.map(upload, payloads))

assert (STORAGE / name).read_bytes() in payloads
assert set(STORAGE.glob(".upload-*")) == temporary_before
print("PASS: five concurrent uploads completed")
print("PASS: final file is one complete upload, with no mixed content")
print("PASS: no additional upload temporary files remain")
print("All file safety tests passed.")
