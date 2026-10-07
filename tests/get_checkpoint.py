import socket
import uuid

ADDRESS = ("127.0.0.1", 9410)
PREFIX = "get-test-" + uuid.uuid4().hex[:12]


def read_exact(reader, size):
    data = bytearray()
    while len(data) < size:
        chunk = reader.read(size - len(data))
        assert chunk, "Connection closed before the complete file arrived"
        data.extend(chunk)
    return bytes(data)


with socket.create_connection(ADDRESS, timeout=5) as sock:
    with sock.makefile("rb") as reader:
        sock.sendall(b"AUTH OPS-3805\n")
        assert reader.readline() == b"OK AUTHENTICATED SID:5083\n"

        for suffix, payload in [
            (".bin", bytes(range(256)) * 80),
            ("-empty.bin", b""),
        ]:
            name = PREFIX + suffix

            sock.sendall(
                f"PUT {name} {len(payload)}\n".encode() + payload
            )
            reply = reader.readline()
            assert reply == (
                f"OK FILE_RECEIVED {name} SID:5083\n".encode()
            ), reply

            # Queue the next command immediately after GET.
            sock.sendall(f"GET {name}\nSYSINFO\n".encode())

            header = reader.readline()
            expected = (
                f"OK FILE_SEND {name} {len(payload)} SID:5083\n".encode()
            )
            assert header == expected, header
            assert read_exact(reader, len(payload)) == payload

            reply = reader.readline()
            assert reply.startswith(b"OK SYSINFO "), reply
            assert reply.endswith(b" SID:5083\n"), reply

            print(f"PASS: GET {len(payload)} bytes matches uploaded content")
            print("PASS: following SYSINFO remains correctly framed")

        sock.sendall(f"GET {PREFIX}-missing.bin\n".encode())
        reply = reader.readline()
        assert reply == b"ERR 005 FILE_NOT_FOUND SID:5083\n", reply
        print("PASS: missing file rejected")

        sock.sendall(b"GET ../escape.bin\n")
        reply = reader.readline()
        assert reply == b"ERR 006 BAD_REQUEST SID:5083\n", reply
        print("PASS: GET path traversal rejected")

        sock.sendall(b"QUIT\n")
        assert reader.readline() == b"OK BYE SID:5083\n"
        assert reader.read(1) == b""
        print("PASS: session remains usable and closes cleanly")

with socket.create_connection(ADDRESS, timeout=5) as sock:
    with sock.makefile("rb") as reader:
        sock.sendall(f"GET {PREFIX}.bin\n".encode())
        reply = reader.readline()
        assert reply == b"ERR 003 AUTH_REQUIRED SID:5083\n", reply
        print("PASS: unauthenticated GET rejected")

print("All GET checkpoint tests passed.")
