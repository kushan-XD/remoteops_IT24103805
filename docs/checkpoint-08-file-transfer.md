# Checkpoint 08: Agent PUT and GET

Implemented Agent-side binary uploads and downloads.
The interactive Controller's file-transfer support is still pending.

Storage: ./agentfiles/IT24103805/
Maximum file size: 64 MiB (67108864 bytes).
Filenames: 1–128 ASCII characters; first character alphanumeric,
remaining characters alphanumeric, dot, underscore or hyphen.

Transfers use exact byte counts through the per-connection buffered
reader. PUT writes a temporary file and atomically renames it after
completion. Failed uploads remove their temporary file and preserve
an existing destination. Concurrent uploads use separate temporary
files; the last successful rename wins.

Rejected PUT requests close the connection to prevent unread body
bytes from being interpreted as commands. GET sends a size header
followed by raw bytes. A failure after that header closes the connection.

Storage directories are opened without following symbolic links.
GET permits regular files only. Transfer socket operations use a
15-second idle timeout; this is not an overall transfer deadline.

Verified:
- Binary and empty uploads and downloads.
- Downloaded bytes match uploaded bytes.
- Following SYSINFO remains correctly framed.
- Incomplete upload cleanup and existing-file preservation.
- Oversized upload rejection.
- PUT and GET path traversal rejection.
- Missing-file GET response.
- Unauthenticated PUT and GET rejection.
- Authentication and text-command regression suites pass.

Symlink rejection, concurrent transfers and timeout expiry still
require dedicated tests.
