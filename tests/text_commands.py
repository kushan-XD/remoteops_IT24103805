import socket

def check_line(reader, prefix):
    line = reader.readline(8193)
    assert len(line) <= 8192, "Response exceeds line limit"
    assert line.startswith(prefix), repr(line)
    assert line.endswith(b" SID:5083\n"), repr(line)
    assert b"\r" not in line and b"\x00" not in line, repr(line)
    return line

with socket.create_connection(("127.0.0.1", 9410), timeout=5) as sock:
    with sock.makefile("rb") as reader:
        sock.sendall(b"AUTH OPS-3805\n")
        assert reader.readline() == b"OK AUTHENTICATED SID:5083\n"

        sock.sendall(b"LISTPROC\n")
        check_line(reader, b"OK PROCS ")
        print("PASS: LISTPROC framing")

        for name in ("DATE", "UPTIME", "DISKFREE", "HOSTNAME", "WHOAMI"):
            sock.sendall(("EXEC " + name + "\n").encode())
            check_line(reader, b"OK EXEC_RESULT ")
            print("PASS:", name)

        for command in ("EXEC UNKNOWN", "EXEC DATE;whoami",
                        "EXEC date", "EXEC DATE extra", "EXEC"):
            sock.sendall((command + "\n").encode())
            actual = reader.readline()
            assert actual == b"ERR 002 COMMAND_NOT_ALLOWED SID:5083\n", repr(actual)
            print("PASS: rejected", repr(command))

        sock.sendall(b"QUIT\n")
        assert reader.readline() == b"OK BYE SID:5083\n"
        assert reader.read(1) == b""

print("PASS: text-command framing, whitelist and clean closure")
