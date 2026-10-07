import socket
import subprocess
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

CONTROLLER = Path("controller_805").resolve()


def run_case(directory, payload, declared_size):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind(("127.0.0.2", 9410))
        server.listen(1)
        server.settimeout(5)

        def serve():
            connection, _ = server.accept()
            with connection:
                connection.settimeout(5)
                with connection.makefile("rb") as reader:
                    assert reader.readline() == b"AUTH OPS-3805\n"
                    connection.sendall(b"OK AUTHENTICATED SID:5083\n")
                    assert reader.readline() == b"GET sample.bin\n"
                    connection.sendall(
                        f"OK FILE_SEND sample.bin {declared_size} "
                        "SID:5083\n".encode() + payload
                    )
                    if len(payload) == declared_size:
                        assert reader.readline() == b"QUIT\n"
                        connection.sendall(b"OK BYE SID:5083\n")

        with ThreadPoolExecutor(max_workers=1) as workers:
            task = workers.submit(serve)
            result = subprocess.run(
                [str(CONTROLLER), "127.0.0.2"],
                input="AUTH OPS-3805\nGET sample.bin\nQUIT\n",
                text=True,
                capture_output=True,
                cwd=directory,
                timeout=10,
            )
            task.result()
            return result


with tempfile.TemporaryDirectory(prefix="remoteops-download-") as directory:
    root = Path(directory)

    result = run_case(directory, b"partial", 100)
    assert result.returncode != 0, result.stdout
    assert not (root / "downloaded-sample.bin").exists()
    assert not list(root.glob(".download-*"))
    print("PASS: truncated download rejected and partial file removed")

    destination = root / "downloaded-sample.bin"
    destination.write_bytes(b"preserve this existing file")

    result = run_case(directory, b"replacement", len(b"replacement"))
    assert result.returncode == 0, (result.stdout, result.stderr)
    assert destination.read_bytes() == b"preserve this existing file"
    assert not list(root.glob(".download-*"))
    print("PASS: existing destination preserved")
    print("PASS: session remains usable after save rejection")

print("All Controller download-safety tests passed.")
