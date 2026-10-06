import select
import socket

ADDRESS = ("127.0.0.1", 9410)

def check_reply(reader, expected):
    actual = reader.readline()
    print(repr(actual))
    assert actual == expected, (actual, expected)


with socket.create_connection(ADDRESS, timeout=5) as sock:
    sock.sendall(b"AUTH OPS-")

    readable, _, _ = select.select([sock], [], [], 0.3)
    assert not readable, "Agent responded before the command was complete"

    sock.sendall(b"3805\nQUIT\n")

    with sock.makefile("rb") as reader:
        check_reply(reader, b"OK AUTHENTICATED SID:5083\n")
        check_reply(reader, b"OK BYE SID:5083\n")
        assert reader.read(1) == b"", "Agent did not close after QUIT"

print("PASS: fragmented AUTH and following QUIT")


with socket.create_connection(ADDRESS, timeout=5) as sock:
    sock.sendall(b"SYSINFO\nAUTH OPS-3805\nQUIT\n")

    with sock.makefile("rb") as reader:
        check_reply(reader, b"ERR 003 AUTH_REQUIRED SID:5083\n")
        check_reply(reader, b"OK AUTHENTICATED SID:5083\n")
        check_reply(reader, b"OK BYE SID:5083\n")
        assert reader.read(1) == b"", "Agent did not close after QUIT"

print("PASS: new connection starts unauthenticated")
