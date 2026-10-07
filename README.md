# RemoteOps — IT24103805

Linux TCP Agent and interactive Controller with authentication,
binary file transfers and per-session UDP monitoring.

## Personalisation

- Registration: IT24103805
- TCP port: 9410
- Agent: agent_805.c
- Controller: controller_805.c
- Build file: Makefile_805
- Authentication token: OPS-3805
- Response identifier: SID:5083
- Log: remoteops_IT24103805.log
- Storage: ./agentfiles/IT24103805/
- Repository: https://github.com/kushan-XD/remoteops_IT24103805

## Build and run

Requirements: Linux, GCC, Make, Python 3 for tests, and the standard
Linux utilities used by LISTPROC and EXEC.
Developed and tested on CentOS Stream 10.

Build from the project directory:

    make -f Makefile_805

Keep all helper headers alongside the C sources.

Terminal A:

    ./agent_805

Terminal B:

    ./controller_805 127.0.0.1

Replace 127.0.0.1 with the Agent's reachable IPv4 address when using
another machine. Recorded tests used local loopback; remote-host
network and firewall configuration was not verified.

## Controller commands

    AUTH OPS-3805
    SYSINFO
    LISTPROC
    EXEC DATE
    EXEC UPTIME
    EXEC DISKFREE
    EXEC HOSTNAME
    EXEC WHOAMI
    PUT sample.bin
    GET sample.bin
    MONITOR START 9500
    MONITOR STOP
    QUIT

Authenticate before issuing other commands.

PUT reads a local file and automatically calculates its size.
The wire header is PUT filename filesize, followed by raw bytes.
GET saves locally as downloaded-filename without overwriting an
existing destination.

Use separate UDP ports, such as 9500 and 9501, for Controllers on the
same machine. Monitoring updates arrive approximately every two
seconds. Their output can interrupt the visible input prompt.

## Protocol and measurement units

TCP command and response headers end with LF. Every OK/ERR response
includes SID:5083. File bodies contain exactly the declared number
of raw bytes. A per-connection buffered reader preserves bytes
received beyond a header.

SYSINFO reports:
- One-minute load average, not CPU utilisation percentage.
- Used memory in MiB: total RAM minus free RAM, including caches.
- System uptime in seconds.

LISTPROC returns a bounded snapshot of up to 50 processes.
Process names may be sanitised.

EXEC accepts only the five exact uppercase names listed above.
User-supplied shell commands are not executed.

UDP updates contain SYSINFO measurements and SID:5083.

## File transfers

Maximum size: 64 MiB.
Filenames: 1–128 ASCII characters, starting with an alphanumeric
character. Later characters may also be dot, underscore or hyphen.
Paths are rejected.

Storage directories are opened without following symbolic links.
GET accepts regular files only.

PUT writes a temporary file and publishes it through atomic rename
after completion. Concurrent uploads to the same filename use
separate temporary files; the last successful rename wins.

Failed uploads remove their temporary files. Rejected PUT requests
close the connection so unread body bytes cannot become commands.
GET failures after the success header close the connection rather
than inserting an error response into the file body.

Agent transfer socket operations have a 15-second idle timeout,
not an overall transfer deadline. The Controller has no equivalent
network timeout.

## Concurrency and logging

Each TCP connection has its own worker, authentication state and
buffered reader. Each monitoring session owns a joinable UDP worker.
STOP and session exit stop and join that worker.

A mutex protects log entries from interleaving. AUTH token values
are omitted. A selected log excerpt is stored in docs/evidence.
Live logs and runtime transfer files are ignored by Git.

## Optional throughput extension

Successful Controller transfers display bytes, elapsed seconds and
MiB/s using CLOCK_MONOTONIC.

MiB/s = (bytes / 1048576) / elapsed_seconds

Timing includes protocol and file I/O. Loopback results do not
measure internet bandwidth.

## Tests

With the Agent running, run from the project directory:

    python3 tests/auth_checkpoint.py
    python3 tests/text_commands.py
    python3 tests/put_checkpoint.py
    python3 tests/get_checkpoint.py
    python3 tests/monitor_checkpoint.py
    python3 tests/file_safety.py
    python3 tests/put_timeout.py

Tests inspecting Agent storage require the tests and Agent to run
from the same project directory on the same machine.

Stop the Agent before running:

    python3 tests/controller_download_safety.py

This mock-server test also binds TCP port 9410.
Restart the Agent afterwards.

Checkpoint notes in docs describe observed results and limitations.

## Known limitations

The fixed authentication token and network traffic are plaintext.
UDP delivery is not guaranteed. Source-IP and SID checks do not
provide cryptographic authentication.

There is no global worker limit or graceful Agent shutdown joining
all client workers. Slow clients can consume resources.
Killing the Agent can leave upload temporary files.


