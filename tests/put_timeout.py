import socket
import time
import uuid
from pathlib import Path

ADDRESS = ("127.0.0.1", 9410)
STORAGE = Path("agentfiles/IT24103805")
name = "timeout-" + uuid.uuid4().hex[:12] + ".bin"
temporary_before = set(STORAGE.glob(".upload-*"))

with socket.create_connection(ADDRESS, timeout=25) as stalled:
    with stalled.makefile("rb") as reader:
        stalled.sendall(b"AUTH OPS-3805\n")
        assert reader.readline() == b"OK AUTHENTICATED SID:5083\n"

        # Promise 100 bytes, send only three, and leave TCP open.
        started = time.monotonic()
        stalled.sendall(f"PUT {name} 100\n".encode() + b"abc")

        # Another session must remain usable during the stalled upload.
        with socket.create_connection(ADDRESS, timeout=5) as other:
            with other.makefile("rb") as other_reader:
                other.sendall(b"AUTH OPS-3805\nSYSINFO\nQUIT\n")
                assert other_reader.readline() == (
                    b"OK AUTHENTICATED SID:5083\n"
                )
                reply = other_reader.readline()
                assert reply.startswith(b"OK SYSINFO "), reply
                assert reply.endswith(b" SID:5083\n"), reply
                assert other_reader.readline() == b"OK BYE SID:5083\n"

        print("PASS: another client works during a stalled upload")

        reply = reader.readline()
        elapsed = time.monotonic() - started
        assert reply == b"ERR 008 IO_ERROR SID:5083\n", reply
        assert reader.read(1) == b""
        assert 12 <= elapsed <= 23, elapsed
        print(f"PASS: stalled upload rejected after {elapsed:.1f} seconds")

assert not (STORAGE / name).exists()
assert set(STORAGE.glob(".upload-*")) == temporary_before
print("PASS: incomplete file and temporary upload removed")
print("All upload-timeout tests passed.")
