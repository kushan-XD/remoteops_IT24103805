# Checkpoint 12: Optional transfer-throughput display

The Controller reports bytes, elapsed seconds and MiB/s after
successful uploads and downloads, using CLOCK_MONOTONIC.

Formula:
MiB/s = (bytes / 1048576) / elapsed_seconds

Timing includes protocol handling and file I/O. It is an end-to-end
application measurement, not a network-only bandwidth benchmark.
The wire protocol is unchanged.

Verified on a 4194304-byte file over 127.0.0.1:
- Upload: 0.058880 seconds, 67.93 MiB/s.
- Download: 0.048873 seconds, 81.84 MiB/s.
- cmp confirmed identical original and downloaded contents.

These values describe one local loopback run and are not guaranteed
performance figures.
