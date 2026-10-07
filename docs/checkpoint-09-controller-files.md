# Checkpoint 09: Controller file transfers

PUT filename calculates the local file size and sends the required
PUT filename filesize header followed by the raw bytes.

GET filename reads the response header and exactly the declared
number of bytes. Downloads are saved as downloaded-filename.
Existing destination files are not overwritten, and incomplete
downloads are removed.

Verified:
- Controller compiled without visible warnings or errors.
- Interactive upload/download round trip completed.
- cmp reported identical original and downloaded files.
- SHA-256 hashes matched.

Existing-destination rejection and interrupted Controller downloads
still require dedicated tests.

Additional verification with tests/controller_download_safety.py:
- Truncated downloads were rejected and partial files removed.
- Existing download destinations were preserved.
- The session remained usable after a save was rejected.

Run this mock-server test with the Agent stopped: both use TCP
port 9410, and the Agent binds all local IPv4 addresses.
Restart the Agent afterwards.
