import socket

ADDRESS = ("127.0.0.1", 9410)


def command(sock, reader, text, expected):
    sock.sendall((text + "\n").encode())
    reply = reader.readline()
    assert reply == expected, (text, reply)


def receive_update(udp):
    udp.settimeout(5)
    packet, source = udp.recvfrom(2048)
    assert source[0] == ADDRESS[0], source
    fields = packet.decode("ascii").split()
    assert len(fields) == 5, packet
    assert fields[0] == "SYSINFO", packet
    assert fields[-1] == "SID:5083", packet
    assert float(fields[1]) >= 0, packet
    assert float(fields[2]) >= 0, packet
    assert int(fields[3]) >= 0, packet
    print("Received:", packet.decode().strip())


def expect_silence(udp):
    # Discard packets already queued before STOP or disconnection.
    udp.setblocking(False)
    while True:
        try:
            udp.recvfrom(2048)
        except BlockingIOError:
            break

    udp.settimeout(3)
    try:
        packet, _ = udp.recvfrom(2048)
    except socket.timeout:
        return
    raise AssertionError(("Unexpected continuing UDP update", packet))


with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp:
    udp.bind(("127.0.0.1", 0))
    port = udp.getsockname()[1]

    with socket.create_connection(ADDRESS, timeout=5) as sock:
        with sock.makefile("rb") as reader:
            command(sock, reader, f"MONITOR START {port}",
                    b"ERR 003 AUTH_REQUIRED SID:5083\n")
            expect_silence(udp)
            print("PASS: authentication required")

            command(sock, reader, "AUTH OPS-3805",
                    b"OK AUTHENTICATED SID:5083\n")

            for invalid in ("0", "65536", "-1", "abc"):
                command(sock, reader, f"MONITOR START {invalid}",
                        b"ERR 006 BAD_REQUEST SID:5083\n")
            print("PASS: invalid UDP ports rejected")

            command(sock, reader, f"MONITOR START {port}",
                    b"OK MONITOR_STARTED SID:5083\n")
            receive_update(udp)
            receive_update(udp)
            print("PASS: repeated UDP updates")

            command(sock, reader, "SYSINFO",
                    # Validate the variable response separately below.
                    ) if False else None
            sock.sendall(b"SYSINFO\n")
            reply = reader.readline()
            assert reply.startswith(b"OK SYSINFO "), reply
            assert reply.endswith(b" SID:5083\n"), reply
            print("PASS: TCP commands work while monitoring")

            command(sock, reader, "MONITOR STOP",
                    b"OK MONITOR_STOPPED SID:5083\n")
            expect_silence(udp)
            print("PASS: STOP ends UDP updates")

            command(sock, reader, f"MONITOR START {port}",
                    b"OK MONITOR_STARTED SID:5083\n")
            receive_update(udp)
            command(sock, reader, "QUIT", b"OK BYE SID:5083\n")
            assert reader.read(1) == b""

    expect_silence(udp)
    print("PASS: QUIT cleans up monitoring")

    with socket.create_connection(ADDRESS, timeout=5) as sock:
        with sock.makefile("rb") as reader:
            command(sock, reader, "AUTH OPS-3805",
                    b"OK AUTHENTICATED SID:5083\n")
            command(sock, reader, f"MONITOR START {port}",
                    b"OK MONITOR_STARTED SID:5083\n")
            receive_update(udp)

            # End TCP input without sending QUIT; wait for Agent cleanup.
            sock.shutdown(socket.SHUT_WR)
            assert reader.read(1) == b""

    expect_silence(udp)
    print("PASS: TCP EOF cleans up monitoring")

print("All monitor checkpoint tests passed.")
